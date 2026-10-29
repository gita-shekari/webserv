#include "Server.hpp"

#include <algorithm>
#include <unistd.h>

// remove an fd from poll registration. Does NOT close the fd.
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
	removePollFd(clientFd); // stop monitoring this client socket
	close(clientFd);
	_clients.erase(it);
}

void	Server::closeListener(int listenerFd)
{
	std::map<int, size_t>::iterator it = _listeners.find(listenerFd);

	if (it == _listeners.end())
		return ;
	
	removePollFd(listenerFd);
	close(listenerFd);
	_listeners.erase(it);
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
		closeListener(listenerFd);
	}

	//clean poll registration
	_pollfds.clear();
}