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
int Server::start(void)
{
	try
	{
		setListeningSockets(); // error happen will throw exception
		runningLoop();
	}
	catch(const std::exception& e)
	{
		return 1;
	}
	return 0;
}

bool	Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags == -1)
	{
		const int	saveErr = errno;
		Logger::systemError(
			Logger::ERROR,
			"fcntl F_GETFL failed",
			saveErr
		);
		return false;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		const int	saveErr = errno;
		Logger::systemError(
			Logger::ERROR,
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
	if (!isListeningFd(fd) && !isClientFd(fd))
	{
		Logger::fatal("event registry invariant violated: fd has no valid owner");
		throw ServerException();
	}
	if (revents & POLLNVAL)
	{
		if (isListeningFd(fd))
		{
			Logger::fatal("listening socket reported POLLNVAL");
			throw ServerException();
		}
		Logger::error("poll returned POLLNVAL: invalid registered fd="
			+ std::to_string(fd) + " action=close_client");
		markForClose(fd);
		return true;
	}
	if (revents & POLLERR)
	{
		if (isListeningFd(fd))
		{
			Logger::fatal("listening socket reported POLLERR");
			throw ServerException();
		}
		Logger::warning("client socket reported POLLERR: fd="
			+ std::to_string(fd) + " action=close_client");
		markForClose(fd);
		return true;
	}
	if (revents & POLLHUP)
	{
		if (isListeningFd(fd))
		{
			Logger::fatal("listening socket reported POLLHUP");
			throw ServerException();
		}
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
                    handleCgiPipeEvent(fd, revents);
                    continue;
                }

				if (!isClientFd(fd))
					continue;
					
				Client& client = getClient(fd);

				if (revents & POLLIN)
				{	
					if (client.getState() == READ_REQ)
					{
						if (!receiveClientData(client))
							continue;
					}
					if (client.getState() == REQ_READY)
					{
						client.routing(_config[client.getServerIndex()]);
					}
					
					
					// routing to handlers (ErrorResponse, CGI, Directory list, upload or delete, redirect, static file ext.)
					//CGI processing
				}
				if (client.getState() == RESPONSE_READY)
				{
					// build serialize
					//TODO: to create 
					enableClientWriteEvent(fd);
                    client.setState(SENDING_REPSONSE);
					_pollfds[i].events |= POLLOUT;
				}
				if (revents & POLLOUT)
				{
					if (client.getState() == SENDING_REPSONSE)
					{
						if (!sendClientData(client))
							continue;
						
						// Only disable POLLOUT when the entire response
                        // has been sent.
						//TODO: to create
                        disableClientWriteEvent(fd);
						_pollfds[i].events &= ~POLLOUT;
					}
					// TODO:: clean up request, parser, and reponse and everything. also check conneciton.
					// if request.headers contains "connection" and "close", mark fd close
				}
			}
		}
		//
		checkCgiProcesses();
		checkTimeouts();
		// need to remove closed fds from pollFds, also check how the macro works with revents.
		removeCloseClient();
	}
}