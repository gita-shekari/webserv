#include "Server.hpp"
#include <fcntl.h> // for non blocking

Server::Server(const std::vector<ServerConfig>& config)
	: _isRunning(false), _config(config)
{
	Logger::debug("server object created");
}

Server::~Server()
{
	shutdown();
	Logger::info("server stopped");
}

int	Server::start()
{
	try
	{
		setListeningSockets();
		runningLoop();
	}
	catch (const std::exception& e)
	{
		Logger::error(e.what());
		shutdown();
		return 1;
	}
	shutdown();
	return 0;
}

void	Server::runningLoop(void)
{
	int	eagainCount = 0;

	while (_isRunning && !stopRequested())
	{
		const size_t polledFdCount = _pollfds.size();
		if (polledFdCount == 0)
			continue;
		// because the size might change since accpet new fds push back in to _pollfds;
		int res = poll(&_pollfds[0], polledFdCount, POLL_TIMEOUT_MS);

		if (stopRequested())
			break;

		if (res == -1)
		{
			handlePollCallError(eagainCount);
			continue;
		}

		eagainCount = 0;
		// TODO: Delete at clean up code at the end: this if res > 0 is not necessory
		if (res > 0)
		{
			for (size_t i = 0; i < polledFdCount && !stopRequested(); i++)
			{
				short revents = _pollfds[i].revents;
				int fd = _pollfds[i].fd;

				if (revents == 0)
					continue;

				if (isListeningFd(fd))
				{
					handleListeningEvent(revents, fd);
					continue;
				}

				if (isClientFd(fd))
				{
					Client& client = getClient(fd);
					handleClientEvent(_pollfds[i], revents, client);
					continue;
				}

				// TODO: to be created
				if (isCgiPipeFd(fd))
				{
					handleCgiPipeEvent(_pollfds[i], revents);
					continue;
				}				
			}
		}
		//TODO:: checkCgiProcesses();
		checkTimeouts();
		removeMarkedClients();
	}
	_isRunning = false;
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

void	Server::addPollFds(int fd, short events)
{
	struct pollfd socket;
	socket.fd = fd;
	socket.events = events;
	socket.revents = 0;
	_pollfds.push_back(socket);
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
