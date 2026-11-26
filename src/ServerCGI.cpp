#include "Server.hpp"
#include <map>
#include <csignal>
#include <sys/wait.h>


// Closing a pipe marks its slot unused. Erasing slots now would skip events in this poll batch.
void Server::removeCgiPipe(int fd)
{
	if (fd == -1)
		return;
	_pipeFds.erase(fd);
	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		if (_pollfds[i].fd == fd)
		{
			_pollfds[i].fd = -1;
			_pollfds[i].events = 0;
			_pollfds[i].revents = 0;
			break;
		}
	}
	close(fd);
}

void Server::cleanupCGI(Client& client)
{
	CGI& cgi = client.getCGI();
	removeCgiPipe(cgi.getInputFd());
	removeCgiPipe(cgi.getOutputFd());
	if (cgi.getPid() > 0 && cgi.getExitStatus() == -1)
	{
		// Never wait for a running script inside the event loop, even during disconnect cleanup.
		kill(cgi.getPid(), SIGKILL);
		_cgiToReap.push_back(cgi.getPid());
	}
	cgi.reset();
}

void Server::prepareClientResponse(Client& client)
{
	if (client.getState() == RESPONSE_READY)
		client.prepareResponseForSending();
	// Pipe events, unlike client events, must explicitly enable the client's writable event.
	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		if (_pollfds[i].fd == client.getFd())
		{
			_pollfds[i].events = POLLOUT;
			break;
		}
	}
}

void Server::failCGI(Client& client, HttpStatus status)
{
	cleanupCGI(client);
	client.handleErrorResponse(_config[client.getServerIndex()], status);
	prepareClientResponse(client);
}

//void Server::handleCgiPipeEvent(pollfd& pfd, short& revents)
//{
//	if (revents & POLLNVAL)
//	{
//		// server side pipefd is closed. but still in pollfds. 
//		// log and cleanCGI, remove from pollfds.
//		/**
//		 * cgistdout -> piperead cannot read from cgi anymore. fd is closed.
//		 * cgistdin -> pipewrite cannot write to cgi anymore. fd is closed.
//		 */ 
//		return ;
//	}
//	if (revents & POLLERR)
//	{
//		// if CGI stdin closed, write end report POLLERR
//		// log, stop writing -> remove from pollfds and close write-end -> continue receiving cgistdout and cgi result.
//		/**
//		 * cgistdout -> piperead errors, check the result of read()
//		 * cgistdin -> is closed pipewrite cannot write into it anymore.  
//		 */
//	}
//	if (revents & POLLHUP)
//	{
//		/** if CGI stdout closed, read end report POLLHUP, need to keep reading until read() = 0, EOF
//		 * cgtstdin -> we closed write to cgi. 
//		 * 
//		*/
// 	}
//	if (revents & (POLLIN | POLLHUP))
//	{
//		// read() from pipe. 
//		/**
//		 * read() > 0: save output and wait to read more
//		 * read() == 0: EOF, remove from pollfds and close readend.
//		 * read() < 0: EAGAIN / EINTR, continue, for next round. other deadful error: CGI failure and cleanup. 
// 		 */
//	}

//	Client& client = getBelongedClient(pfd.fd);
//	//client.checkPIDprocess();
//	// TODO:: check whether the pipe is finished.
//	// this function will change client state to RESPONSE_READY or keep it as CGI_RUNNING.
//	// in routing, handleCGI() will change client status to CGI_RUNNING or error happened before setting up process, it will give handleErrorResponse(), which switch the state to RESPONSE_READY.

//}

void Server::handleCgiPipeEvent(pollfd& pfd, short& revents)
{
	Client& client = getBelongedClient(pfd.fd);
	if (!client.getIsMarkedForClose() || client.getState() != CGI_RUNNING)
		return;
	CGI& cgi = client.getCGI();
	if (revents & POLLNVAL)
		return failCGI(client, BAD_GATEWAY);
	if (pfd.fd == cgi.getInputFd())
	{
		// A script is allowed to finish without consuming all stdin; stop writing on EPIPE/HUP.
		if (revents & (POLLERR | POLLHUP))
		{
			removeCgiPipe(cgi.getInputFd());
			cgi.setInputFd(-1);
			return;
		}
		if (!(revents & POLLOUT))
			return;
		const std::string& body = client.getRequestBody();
		size_t remaining = body.size() - cgi.getBodyOffset();
		// A bounded write keeps one large POST from monopolizing the loop.
		ssize_t size = write(cgi.getInputFd(), body.data() + cgi.getBodyOffset(), std::min(remaining, size_t(16384)));
		if (size > 0)
			cgi.setBodyOffset(cgi.getBodyOffset() + size);
		else if (size == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		{
			if (errno != EPIPE)
				return failCGI(client, BAD_GATEWAY);
			removeCgiPipe(cgi.getInputFd());
			cgi.setInputFd(-1);
			return;
		}
		if (cgi.getBodyOffset() == body.size())
		{
			// Python's stdin.read() needs EOF after the last body byte.
			removeCgiPipe(cgi.getInputFd());
			cgi.setInputFd(-1);
		}
		return;
	}
	if (revents & (POLLIN | POLLHUP | POLLERR))
	{
		// HUP can arrive together with unread bytes. Drain output before treating it as EOF.
		char buffer[16384];
		ssize_t size = read(cgi.getOutputFd(), buffer, sizeof(buffer));
		if (size > 0)
		{
			// Bound buffered output so a runaway script cannot consume unlimited server memory.
			if (cgi.getOutput().size() + size > 10 * 1024 * 1024)
				return failCGI(client, BAD_GATEWAY);
			cgi.appendOutput(buffer, size);
		}
		else if (size == 0)
		{
			removeCgiPipe(cgi.getOutputFd());
			cgi.setOutputFd(-1);
		}
		else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			failCGI(client, BAD_GATEWAY);
	}
}

void Server::registerCgiPipes(Client& client)
{
	CGI& cgi = client.getCGI();
	// This flag also tells the send path to close this CGI connection after the response.
	client.setCgiPipesRegistered(true);
	if (cgi.getInputFd() == -1 || cgi.getOutputFd() == -1)
		return failCGI(client, INTERNAL_SERVER_ERR);
	// CGI::start already made the parent's pipe ends non-blocking.
	if (client.getRequestBody().empty())
	{
		// GET and empty POST requests must immediately give the script stdin EOF.
		removeCgiPipe(cgi.getInputFd());
		cgi.setInputFd(-1);
	}
	else
	{
		_pipeFds[cgi.getInputFd()] = client.getFd();
		addPollFds(cgi.getInputFd(), POLLOUT);
	}
	_pipeFds[cgi.getOutputFd()] = client.getFd();
	addPollFds(cgi.getOutputFd(), POLLIN);
}

void Server::checkCgiProcesses()
{
	// Retry reaping killed processes on subsequent iterations, without blocking other requests.
	for (std::vector<pid_t>::iterator it = _cgiToReap.begin(); it != _cgiToReap.end();)
	{
		int status;
		pid_t result = waitpid(*it, &status, WNOHANG);
		if (result > 0 || (result == -1 && errno == ECHILD))
			it = _cgiToReap.erase(it);
		else
			++it;
	}
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		Client& client = it->second;
		if (!client.getIsMarkedForClose() || client.getState() != CGI_RUNNING)
			continue;
		CGI& cgi = client.getCGI();
		if (cgi.getExitStatus() == -1)
		{
			int status;
			pid_t result = waitpid(cgi.getPid(), &status, WNOHANG);
			if (result > 0)
				cgi.setExitStatus(status);
			else if (result == -1 && errno != EINTR)
			{
				cgi.setPid(-1);
				failCGI(client, BAD_GATEWAY);
				continue;
			}
		}
		int exitStatus = cgi.getExitStatus();
		if (exitStatus != -1 && (!WIFEXITED(exitStatus) || WEXITSTATUS(exitStatus) != 0))
		{
			failCGI(client, BAD_GATEWAY);
			continue;
		}
		// Both EOF and successful process exit are needed: either can happen first.
		if (cgi.getOutputFd() == -1 && cgi.getExitStatus() != -1)
		{
			client.finishCGI(_config[client.getServerIndex()]);
			cleanupCGI(client);
			prepareClientResponse(client);
			continue;
		}
		std::chrono::seconds elapsed = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::steady_clock::now() - cgi.getStartedAt());
		if (elapsed.count() >= CGI_TIMEOUT_SEC)
			failCGI(client, GATEWAY_TIMEOUT);
	}
}
