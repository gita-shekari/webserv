#include "Server.hpp"
#include <algorithm>
#include <cerrno>

//TODO: no use
bool Server::getRunning(void)
{
	return this->_isRunning;
}

Client& Server::getClient(int clientFd)
{
	std::map<int, Client>::iterator it = _clients.find(clientFd);
	if (it == _clients.end())
	{
		Logger::fatal("event registry invariant violated: poll fd="
			+ std::to_string(clientFd) + " has no Client");
		throw ServerException();
	}
	return (it->second);
}

bool	Server::isClientFd(int fd) const
{
	return _clients.find(fd) != _clients.end();
}

bool	Server::isListeningFd(int fd) const
{
	return _listeners.find(fd) != _listeners.end();
}

void	Server::markForClose(int fd)
{
	Logger::debug("marking client for close: fd=" + std::to_string(fd));
	// shouldn't do this because it might create a client if it doent exist;
	//_clients[fd].Client::disConnected();
	Client& client = getClient(fd);
	client.disConnected();
}

void	Server::removeCloseClient(void)
{
	std::map<int, Client>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		if (!it->second.getIsConnected())
		{
			int closeFd = it->second.getFd();
			std::vector<struct pollfd>::iterator pfdIt = _pollfds.begin();
			while (pfdIt != _pollfds.end())
			{
				if (pfdIt->fd == closeFd)
				{
					pfdIt = _pollfds.erase(pfdIt);
					break;
				}
				else
				{
					++pfdIt;
				}
			}
			it = _clients.erase(it);
			close(closeFd);
		}
		else
		{
			++it;
		}
	}
}

void	Server::checkTimeouts(void)
{
	std::chrono::steady_clock::time_point now
		 = std::chrono::steady_clock::now();
	for (std::map<int, Client>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		std::chrono::seconds idleTime =
			std::chrono::duration_cast<std::chrono::seconds>(
				now - it->second.getLastActivity());
		if (idleTime.count() >= CLIENT_TIMEOUT_SEC)
		{
			Logger::info(
				"Client timeout: fd=" + std::to_string(it->first)
				+ " action=close_client"
			);
			markForClose(it->first);
		}
	}
}

const char* Server::ServerException::what() const noexcept
{
	return "";
}
