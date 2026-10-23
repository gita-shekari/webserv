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

void Server::handleEvent(pollfd& pfd, short& revents, Client& client)
{
	if (revents & POLLIN)
	{
		if (client.getState() == READ_REQ)
		{
			if (!receiveClientData(client))
				return ; // continue the loop
		}
		if (client.getState() == REQ_READY)
		{
			client.routing(_config[client.getServerIndex()]);
		}
		if (client.getState() == CGI_RUNNING)
		{
			registerCgiPipes(client);
		}
	}
	if (client.getState() == RESPONSE_READY)
	{
		client.prepareResponseForSending();
		pfd.events |= POLLOUT;

	}
	if (revents & POLLOUT)
	{
		if (client.getState() == SENDING_RESPONSE)
		{
			if (!sendClientData(client))
				return ; // continue the loop

			// Only disable POLLOUT when the entire response
			// has been sent.
			//TODO: to create
			pfd.events &= ~POLLOUT;
		}
		// TODO:: clean up request, parser, and reponse and everything. also check conneciton.
		// if request.headers contains "connection" and "close", mark fd close
	}
}

void Server::handleCgiPipeEvent(pollfd& pfd, short& revents)
{
	Client& client = getBelongedClient(pfd.fd);
	if (client.getState() != CGI_RUNNING) // check
		return handleEvent(pfd, revents, client);

	// TODO:: check whether the pipe is finished.
	// this function will change client state to RESPONSE_READY or keep it as CGI_RUNNING.
	// in routing, handleCGI() will change client status to CGI_RUNNING or error happened before setting up process, it will give handleErrorResponse(), which switch the state to RESPONSE_READY.

}

void	Server::registerCgiPipes(Client& client)
{
	// here I am gonna add pipfd and clientfd to the _pipfd
	int inputFd = client.getCgiInputFd();
	int outputFd = client.getCgiOutputFd();
	int clientFd = client.getFd();
	_pipeFds[inputFd] = clientFd;
	_pipeFds[outputFd] = clientFd;
	//I also need to call this function only once for a request, by adding a flag or sth I now I registerd it already
	Logger::debug("registered CGI input fd=" + std::to_string(inputFd) + " for client fd=" + std::to_string(clientFd));
	Logger::debug("registered CGI output fd=" + std::to_string(outputFd) + " for client fd=" + std::to_string(clientFd));
}
