#include "Server.hpp"

#include <algorithm>
#include <unistd.h>

void	Server::markForClose(int fd)
{
	Logger::debug("marking client for close: fd=" + std::to_string(fd));
	// shouldn't do this because it might create a client if it doent exist;
	//_clients[fd].Client::setIsMarkedForClose();
	Client& client = getClient(fd);
	client.setIsMarkedForClose();
}

void	Server::removeMarkedClients(void)
{
	std::map<int, Client>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		if (!it->second.getIsMarkedForClose())
		{
			int fd = it->first;
			++it;
			closeClient(fd);
		}
		else
			++it;
	}
}

void	Server::shutdown()
{
	// 1. stop the running loop
	_isRunning = false;

	// 2. close all clients and their associated CGI resources
	while (!_clients.empty())
	{
		int clientFd = _clients.begin()->first;
		closeClient(clientFd);
	}

	// 3. close any remaining registered CGI pipes
	while (!_pipeFds.empty())
	{
		int pipeFd = _pipeFds.begin()->first;

		removePollFd(pipeFd);
		close(pipeFd);

		_pipeFds.erase(_pipeFds.begin());
	}

	// 4. close all listening sockets
	while (!_listeners.empty())
	{
		int listenerFd = _listeners.begin()->first;
		
		removePollFd(listenerFd);
		close(listenerFd);

		_listeners.erase(_listeners.begin());
	}

	//clean poll registration
	_pollfds.clear();
}


// remove an fd from poll registration. =stop monitoring; Does NOT close the fd.
void	Server::removePollFd(int fd)
{
	for (std::vector<pollfd>::iterator it = _pollfds.begin();
		it != _pollfds.end(); )
	{
		if (it->fd == fd)
			it = _pollfds.erase(it);
		else
			++it;
	}
}

void	Server::closeClient(int clientFd)
{
	std::map<int, Client>::iterator it = _clients.find(clientFd);

	if (it == _clients.end())
		return ;
	
	cleanupCgi(clientFd);
	removePollFd(clientFd);
	close(clientFd);
	_clients.erase(it);
}

void	Server::cleanupCgi(int clientFd)
{
	std::map<int, int>::iterator it = _pipeFds.begin();

	while (it != _pipeFds.end())
	{
		if (it->second == clientFd)
		{
			int pipeFd = it->first;

			removePollFd(pipeFd);
			close(pipeFd);

			it = _pipeFds.erase(it);
		}
		else
			++it;
	}
}