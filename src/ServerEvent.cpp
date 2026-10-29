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
			//if (client.getState() == CGI_RUNNING)
			//{
			//	_pipeFds[client.getPipefd()] = client.getFd();
			//	_pollfds.push_back(client.getPipefd());
			//}
		}
		// routing to handlers (ErrorResponse, CGI, Directory list, upload or delete, redirect, static file ext.)
		//CGI processing
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
	if (client.getState() != CGI_RUNNING)
		return handleEvent(pfd, revents, client);
	//client.checkPIDprocess();
	// TODO:: check whether the pipe is finished.
	// this function will change client state to RESPONSE_READY or keep it as CGI_RUNNING.
	// in routing, handleCGI() will change client status to CGI_RUNNING or error happened before setting up process, it will give handleErrorResponse(), which switch the state to RESPONSE_READY.

}
