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
	}
	if (client.getState() == CGI_RUNNING && !client.getCgiPipesRegistered())
	{
		registerCgiPipes(client);
		return; // Adding pipes can reallocate the poll vector.
	}
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
	if (revents & (POLLNVAL | POLLERR))
	{
		markForClose(client.getFd());
		return;
	}

	// HUP can accompany unread request bytes or a client's write-side shutdown.
	if ((revents & (POLLIN | POLLHUP)) && !client.getPeerReadClosed())
		receiveClientData(client);
	if (client.getIsMarkedForClose())
		return ;
	if (client.getPeerReadClosed())
		pfd.events &= ~POLLIN;
	// means during waiting for processing finished request and client close its sending to Server. Then we deregister POLLIN and no longer listening to this return event.

	if ((revents & POLLOUT)
		|| ((revents & POLLHUP) && client.getState() == SENDING_RESPONSE))
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
	if (client.getPeerReadClosed() && client.getState() == CGI_RUNNING)
	{
		// Retire a hung-up socket while CGI runs to avoid a busy poll loop.
		// prepareClientResponse registers it again when output is ready.
		pfd.fd = -1;
		pfd.events = 0;
	}
	// process Client request: 1. parse, 2. routing, 3. prepare response.
	processClientRequest(pfd, client);
}
