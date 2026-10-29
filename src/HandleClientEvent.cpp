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
				return;
		}
		if (client.getState() == REQ_READY)
			client.routing(_config[client.getServerIndex()]);
	}

	if (client.getState() == CGI_RUNNING && !client.getCgiPipesRegistered())
		registerCgiPipes(client);

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
				return;

			pfd.events &= ~POLLOUT;
		}
	}
}

void Server::handleCgiPipeEvent(pollfd& pfd, short& revents)
{
	Client& client = getBelongedClient(pfd.fd);
	if (client.getState() != CGI_RUNNING || !client.getCgiPipesRegistered())
		return;
	if (pfd.fd == client.getCGI().getInputFd())
	{
		if (revents & POLLOUT)
		{
			Logger::debug("CGI input pipe writable: fd=" + std::to_string(pfd.fd));
			// next: write request body
		}
	}

	if (pfd.fd == client.getCGI().getOutputFd())
	{
		if (revents & POLLIN)
		{
			Logger::debug("CGI output pipe readable: fd=" + std::to_string(pfd.fd));
			// next: read CGI output
		}
	}
}

void	Server::registerCgiPipes(Client& client)
{
	int inputFd = client.getCGI().getInputFd();
	int outputFd = client.getCGI().getOutputFd();
	int clientFd = client.getFd();
	if (inputFd == -1 || outputFd == -1)
	{
		Logger::error("cannot register CGI pipes: invalid fd");
		return;
	}
	if (!setNonBlocking(inputFd) || !setNonBlocking(outputFd))
	{
		Logger::error("cannot register CGI pipes: failed to set non-blocking");
		return;
	}
	_pipeFds[inputFd] = clientFd;
	_pipeFds[outputFd] = clientFd;
	addPollFds(inputFd, POLLOUT);
	addPollFds(outputFd, POLLIN);
	client.setCgiPipesRegistered(true);
	Logger::debug("registered CGI input fd=" + std::to_string(inputFd) + " for client fd=" + std::to_string(clientFd));
	Logger::debug("registered CGI output fd=" + std::to_string(outputFd) + " for client fd=" + std::to_string(clientFd));
}
