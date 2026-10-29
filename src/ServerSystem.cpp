#include "Server.hpp"
// for non blocking
#include <fcntl.h>

Server::Server(const std::vector<ServerConfig>& config)
	: _config(config)
{
	Logger::debug("server object created");
}
// Need server shutdown function.
Server::~Server(void)
{
	for (std::map<int, size_t>::iterator it = _listeners.begin();
		it != _listeners.end();
		++it)
		close(it->first);

	for (std::map<int, Client>::iterator it = _clients.begin();
		it != _clients.end();
		++it)
		close(it->first);
	// not only close socket, also need to check pullfd and clients in case of throw exception destructor.
	Logger::info("server stopped");
}

// do we need to return as int?
void Server::start(void)
{
	setListeningSockets();
	runningLoop();
}

bool	Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags == -1)
	{
		const int	saveErr = errno;
		Logger::systemError(
			Logger::FATAL,
			"fcntl F_GETFL failed",
			saveErr
		);
		return false;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		const int	saveErr = errno;
		Logger::systemError(
			Logger::FATAL,
			"fcntl F_SETFL O_NONBLOCK failed",
			saveErr
		);
		return false;
	}
	return true;
}

void	Server::handlePollCallError(int& eagainCount)
{
	const int saveErr = errno;

	if (saveErr == EINTR)
	{
		Logger::debug("poll interrupted; retrying");
		return;
	}

	if (saveErr == EAGAIN || saveErr == ENOMEM)
	{
		if (eagainCount > 2)
		{
			Logger::systemError(Logger::FATAL,
				"poll: resource exhaustion after retries", saveErr);
			throw ServerException();
		}
		eagainCount++;
		Logger::systemError(Logger::WARNING,
			"poll: temporary resource exhaustion; retrying", saveErr);
		return;
	}
	Logger::systemError(Logger::FATAL, "poll failed", saveErr);
	throw ServerException();
}

bool	Server::handlePollException(int fd, short revents)
{
	if (!(revents & (POLLNVAL | POLLERR | POLLHUP)))
		return false; // no exceptions found, continue with runningLoop()
	// TODO: This need to be changed. because it might be the pipefd. 
	if (!isListeningFd(fd) && !isClientFd(fd))
	{
		Logger::fatal("event registry invariant violated: fd has no valid owner");
		throw ServerException();
	}
	if (isListeningFd(fd))
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
	if (revents & POLLNVAL)
	{
		Logger::error("poll returned POLLNVAL: invalid registered fd="
			+ std::to_string(fd) + " action=close_client");
		markForClose(fd);
		return true;
	}
	if (revents & POLLERR)
	{
		Logger::warning("client socket reported POLLERR: fd="
			+ std::to_string(fd) + " action=close_client");
		markForClose(fd);
		return true;
	}
	if (revents & POLLHUP)
	{
		Logger::info("client socket hangup: fd="
			+ std::to_string(fd) + " action=close_client");
		markForClose(fd);
		return true;
	}
	return false;
}

void	Server::addPollFds(int fd, short events)
{
	struct pollfd socket;
	socket.fd = fd;
	socket.events = events;
	socket.revents = 0;
	_pollfds.push_back(socket);
}

void	Server::runningLoop(void)
{
	int	eagainCount = 0;

	while (_isRunning)
	{
		const size_t polledFdCount = _pollfds.size();
		if (polledFdCount == 0)
			continue;
		// because the size might change since accpet new fds push back in to _pollfds;
		int res = poll(&_pollfds[0], polledFdCount, POLL_TIMEOUT_MS);

		if (res == -1)
		{
			handlePollCallError(eagainCount);
			continue;
		}

		eagainCount = 0;

		if (res > 0)
		{
			for (size_t i = 0; i < polledFdCount; i++)
			{
				short revents = _pollfds[i].revents;
				int fd = _pollfds[i].fd;

				if (revents == 0)
					continue;

				if (handlePollException(fd, revents))
					continue;

				if (isListeningFd(fd))
				{
					if (revents & POLLIN)
						acceptNewClient(fd);
					continue;
				}

				// TODO: to be created
				if (isCgiPipeFd(fd))
				{
					handleCgiPipeEvent(_pollfds[i], revents);
					continue;
				}

				if (!isClientFd(fd))
					continue;

				Client& client = getClient(fd);
				handleEvent(_pollfds[i], revents, client);
			}
		}
		//
		//TODO:: checkCgiProcesses();
		checkTimeouts();
		// need to remove closed fds from pollFds, also check how the macro works with revents.
		removeCloseClient();
	}
}
