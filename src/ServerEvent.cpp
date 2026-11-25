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
		receiveClientData(client); // receiving data and deal with EOF
	if (client.getIsMarkedForClose())
		return ; 
	if (client.getPeerReadClosed())
		pfd.events &= ~POLLIN;// means during waiting for processing finished request and client close its sending to Server. Then we deregister POLLIN and no longer listening to this return event. 

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
	if (client.getState() == RESPONSE_READY)
	{
		client.prepareResponseForSending();
		pfd.events |= POLLOUT;
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
