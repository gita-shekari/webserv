#include "Server.hpp"
#include <map>

bool	Server::isCgiPipeFd(int fd)
{
	std::map<int, int>::const_iterator it = _pipeFds.find(fd);
	if (it == _pipeFds.end())
		return false;
	return true;
}

Client& Server::getBelongedClient(int fd)
{
	std::map<int, int>::const_iterator it = _pipeFds.find(fd);
	if (it == _pipeFds.end())
	{
		Logger::fatal("event registry invariant violated: pollfd as pipefd has no valid owner");
		throw ServerException();
	}
	int clientFd = it->second;
	return (getClient(clientFd));
}

void	Server::processClientRequest(pollfd& pfd, Client& client)
{
	if (client.getState() == READ_REQ)
	{
		ParseStatus status = parseClientRequest(client);
		if (status == INCOMPLETE)
		{
			if (client.getPeerReadClosed())
				markForClose(client.getFd());
			return ;
		}
		else if (status == ERROR)
			client.setCloseAfterResponse();
		client.setState(REQ_READY);
	}
	if (client.getState() == REQ_READY)
	{
		client.routing(_config[client.getServerIndex()]);
		//if (client.getState() == CGI_RUNNING) // better to put at handleCGI after the cgi process is being changed. 
		//{
		//	_pipeFds[client.getPipefd()] = client.getFd();
		//	_pollfds.push_back(client.getPipefd());
		//}
	}
	/////////TODO: Gita add this
	if (client.getState() == CGI_RUNNING && !client.getCgiPipesRegistered())
	{
		// Adding pipes can reallocate _pollfds: do not use pfd again afterward.
		pfd.events = 0;
		registerCgiPipes(client);
		return;
	}
	//////

	if (client.getState() == RESPONSE_READY)
	{
		client.prepareResponseForSending();
		pfd.events |= POLLOUT;
	}
}

void Server::handleListeningEvent(short revents, int fd)
{
	if (revents & (POLLNVAL | POLLERR))
	{
		std::string event;
		if (revents & POLLNVAL)
			event = "POLLNVAL";
		else if (revents & POLLERR)
			event = "POLLERR";
		else
			event = "POLLHUP";
		Logger::fatal("listening socket reported at poll() as " + event + ".\n");
		throw ServerException();
	}
	if (revents & POLLIN)
		acceptNewClient(fd);
}

void Server::handleClientEvent(pollfd& pfd, short& revents, Client& client)
{
	if (revents & (POLLNVAL | POLLERR | POLLHUP))
	{
		if (revents & POLLNVAL)
			Logger::error("poll returned POLLNVAL: invalid registered fd="
			+ std::to_string(client.getFd()) + " action=close_client");
		else if (revents & POLLERR)
			Logger::warning("client socket reported POLLERR: fd="
			+ std::to_string(client.getFd()) + " action=close_client");
		else
			Logger::info("client socket hangup: fd="
			+ std::to_string(client.getFd()) + " action=close_client");	
		markForClose(client.getFd());
		return ;
	}

	if (revents & POLLIN)
		receiveClientData(client); // receiving data and deal with EOF
	if (client.getIsMarkedForClose())
		return ; 
	if (client.getPeerReadClosed())
		pfd.events &= ~POLLIN;
	// means during waiting for processing finished request and client close its sending to Server. Then we deregister POLLIN and no longer listening to this return event. 

	if (revents & POLLOUT)
	{
		if (client.getState() != SENDING_RESPONSE)
		{
			pfd.events &= ~POLLOUT;
			Logger::error("POLLOUT received without a response to send");
        	return ;
		}

		if (!sendClientData(client))
			return ; // continue the loop

		pfd.events &= ~POLLOUT;
		/////// TODO: gita add this
		// CGI responses use Connection: close; close only after the full buffer is sent.
		if (client.getCgiPipesRegistered())
			markForClose(client.getFd());

		///////
		if (client.getIsMarkedForClose())
			return ;
		if (client.getPeerReadClosed())
		{
			markForClose(client.getFd());
			return ;
		}
		if (!client.resetClient())
		{
			markForClose(client.getFd());
			return ; 
		}
	}
	// process Client request: 1. parse, 2. routing, 3. prepare response.
	processClientRequest(pfd, client);
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
