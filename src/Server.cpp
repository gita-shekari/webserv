#include "Server.hpp"
// for socket;
#include <netinet/in.h> 
#include <sys/socket.h>
#include <unistd.h> // for closing socket
#include <vector>
#include <algorithm>
#include <cerrno>

Server::Server(struct ServerConfig& config)
	: _socketFd(-1), 
	  _isRunning(false),
	  _config(config)
{
	Logger::debug("server object created");
}
// Need server shutdown function.
Server::~Server(void)
{
	if (this->_socketFd != -1)
		close(this->_socketFd);
	// not only close socket, also need to check pullfd and clients in case of throw exception destructor.
	Logger::info("server stopped");
}

bool Server::getRunning(void)
{
	return this->_isRunning;
}

int	Server::getSocketFd(void)
{
	return this->_socketFd;
}

void	Server::markForClose(int fd)
{
	Logger::debug("marking client for close: fd=" + std::to_string(fd));
	// shouldn't do this because it might create a client if it doent exist;
	//_clients[fd].Client::disConnected();

	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it != _clients.end())
	{
		it->second.disConnected();
	}
	else
	{
		Logger::fatal("client registry invariant violated: cannot close unknown fd="
			+ std::to_string(fd));
		throw ServerException();
	}
}

void	Server::acceptNewClient(void)
{
	//struct sockaddr_in clientAddr;
	//socklen_t len = sizeof(clientAddr);
	
	int clientFd = accept(_socketFd, NULL, NULL);
	if (clientFd == -1)
	{
		const int errorNumber = errno;
		// if we set socket as O_NONBLOCK, then no connection is ready can return -1
		if (errorNumber == EAGAIN || errorNumber == EWOULDBLOCK)
			return;
		if (errorNumber == EINTR)
			Logger::debug("accept interrupted; retry on the next event");
		else if (errorNumber == EMFILE || errorNumber == ENFILE)
			Logger::systemError(Logger::ERROR, "accept: file descriptor limit reached", errorNumber);
		else if (errorNumber == EBADF || errorNumber == ENOTSOCK)
		{
			Logger::systemError(Logger::FATAL, "accept: invalid listening socket", errorNumber);
			throw ServerException();
		}
		else
			Logger::systemError(Logger::WARNING, "accept failed", errorNumber);
		return ;
	}

	struct pollfd client;
	client.fd = clientFd;
	client.events = POLLIN;
	client.revents = 0;
	_pollfds.push_back(client);

	// create instance of client/connection class.

	//_clients.insert(std::make_pair(clientFd, Client(clientFd))); --> because we have a default consructor for client, we don't have to use make_pair and insert. we can just use map operator[] because when there is no such key, it would create one and copy the Client(clientFd) into it, the Client(clientFd) will be destroyed after the this line. 
	_clients[clientFd] = Client(clientFd);

	Logger::debug("client connected: fd=" + std::to_string(clientFd));
}

template <typename ClientsIt>
ReceiveStatus	Server::receiveClientData(int fd, ClientsIt it)
{
	char buffer[1024] = {0};

	ssize_t bytesReceived = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (bytesReceived > 0)
	{
		Logger::debug("received client data: fd=" + std::to_string(fd)
			+ " bytes=" + std::to_string(bytesReceived));
		
		ParseStatus status = it->second.parseRequest(buffer);
		if (status == COMPLETE)
			return DONE;
		else if (status == INCOMPLETE)
			return 	CONTINUE;
		else if (status == ERROR)
			return ERROR;

	}
	else if (bytesReceived == 0)
	{
		Logger::debug("client closed connection: fd=" + std::to_string(fd));
		markForClose(fd);
		return CONTINUE;
	}
	else
	{
		const int errorNumber = errno;
		if (errorNumber == EAGAIN || errorNumber == EWOULDBLOCK)
			return CONTINUE;
		if (errorNumber == ECONNRESET || errorNumber == ETIMEDOUT)
		{
			markForClose(fd);
			Logger::systemError(Logger::INFO, "recv: client connection ended", errorNumber);
		}
		else if (errorNumber == EINTR)
			Logger::debug("recv interrupted; retry on the next event");
 		else
		{
			Logger::systemError(Logger::ERROR, "recv failed", errorNumber);
			markForClose(fd);
		}
		return CONTINUE;
	}
}

bool	Server::sendClientData(int fd)
{
	Logger::debug("response ready to send: fd=" + std::to_string(fd));

	// also need to remove from pollfds and another things. 
	if (fd < 0) // if finish sending the data, then return true.
		return false;
	else
		return true;
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

void	Server::runningLoop(void)
{
	int	eagainCount = 0;
	while (_isRunning)
	{
		int res = poll(&_pollfds[0], _pollfds.size(), -1); //timeout?

		if (res == 0)// timeout, no event happend;
			continue;
		else if (res == -1)
		{
			const int errorNumber = errno;
			if (errorNumber == EINTR)
			{
				Logger::debug("poll interrupted; retrying");
				continue;
			}
			else if (errorNumber == EAGAIN || errorNumber == ENOMEM)
			{
				if (eagainCount > 2)
				{
					Logger::systemError(Logger::FATAL,
						"poll: resource exhaustion after retries", errorNumber);
					throw ServerException();
				}
				else
				{
					eagainCount++;
					Logger::systemError(Logger::WARNING,
						"poll: temporary resource exhaustion; retrying", errorNumber);
					continue;
				}
			}
			else
			{
				Logger::systemError(Logger::FATAL, "poll failed", errorNumber);
				throw ServerException();				
			}
		}

		for (size_t i = 0; i < _pollfds.size(); i++)
		{
			short revents = _pollfds[i].revents;
			int fd = _pollfds[i].fd;
			
			// check revent error firstly;
			if (revents == 0)
			{
				//checkTimeouts();
				continue;
			}
			if (revents & POLLNVAL)
			{
				Logger::error("poll returned POLLNVAL: invalid registered fd="
					+ std::to_string(fd) + " action=close_client");
				if (fd == _socketFd || _clients.find(fd) == _clients.end())
				{
					Logger::fatal("event registry invariant violated: fd has no valid owner");
					throw ServerException();
				}
				markForClose(fd);
				continue;
			}
			if (revents & POLLERR)
			{
				if (fd == _socketFd)
				{
					Logger::fatal("listening socket reported POLLERR");
					throw ServerException();
				}
				Logger::warning("client socket reported POLLERR: fd=" + std::to_string(fd)
					+ " action=close_client");
				markForClose(fd);
				continue;
			}
			if (revents & POLLHUP)
			{
				if (fd == _socketFd)
				{
					Logger::fatal("listening socket reported POLLHUP");
					throw ServerException();
				}
				Logger::info("client socket hangup: fd=" + std::to_string(fd)
					+ " action=close_client");
				markForClose(fd);
				continue;
			}

			if (fd == _socketFd)
			{
				if (revents & POLLIN)
				{
					acceptNewClient();
				}
			}
			else
			{
				if (revents & POLLIN)
				{
					std::map<int, Client>::iterator it = _clients.find(fd);
					if (it == _clients.end())
					{
						Logger::fatal("event registry invariant violated: poll fd="
							+ std::to_string(fd) + " has no Client");
						throw ServerException();
					}

					ReceiveStatus	status = receiveClientData(fd, it);
					if (status == DONE)
					{
						
						_pollfds[i].events |= POLLOUT;

						// 1. check if we need to run static things? or CGI 
						// 2. build repsonse based on the return of step 1. 
						//it->second.getReq().
						// clear client's Request
					}
					if (status == ERROR)
					{
						// check errtype and buildErrResponse
						// it->second.getReq().errtype ： the enum has a number as status code, can be used directly to response. 
						// clear client's Request
					}
					if (status == CONTINUE)
						continue;
				}
				if (revents & POLLOUT)
				{
					if (sendClientData(fd))
					{
						_pollfds[i].events &= ~POLLOUT;
					}
				}
			}
			//checkTimeouts();
		}
		// need to remove closed fds from pollFds, also check how the macro works with revents.
		removeCloseClient();
		eagainCount = 0;
	}
}

void	Server::setPollFds(void)
{
	struct pollfd socket;
	socket.fd = _socketFd;
	socket.events = POLLIN;
	socket.revents = 0;
	_pollfds.push_back(socket);
}

void	Server::setsocket(void)
{
	this->_socketFd = socket(AF_INET, SOCK_STREAM, 0);
	//AF_INET : IPv4 protocol SOCK_STREAM: TCP socket
	// this is for linux, do we need to write a seperate one for windows? how to decide which one to use?

	if (this->_socketFd == -1)
	{
		const int errorNumber = errno;
		Logger::systemError(Logger::FATAL, "socket creation failed", errorNumber);
		/** ERROR TYPES -- for now we only log, not diff them 
		 * ❌ Invalid Arguments (Configuration Errors)
		 * EAFNOSUPPORT (Linux) / WSAEAFNOSUPPORT (Windows): The specified address family (e.g., AF_INET, AF_INET6) is not supported by the OS implementation or network stack.
		 * EPROTONOSUPPORT / WSAEPROTONOSUPPORT: The requested network protocol is not supported within this domain (e.g., trying to use an invalid protocol number for a SOCK_STREAM socket).
		 * EINVAL / WSAEINVAL: General invalid flags or an unknown protocol combination passed into the function parameters. 
		 * 🛑 Resource Exhaustion (System-Level Limits)
		 * EMFILE / WSAEMFILE: 
		 * The process has hit its maximum cap for open file descriptors. Your program has too many open files or sockets and cannot allocate a new one.
		 * ENFILE: The entire operating system has run out of open file allocations globally.
		 * ENOBUFS or ENOMEM / WSAENOBUFS: The kernel lacks insufficient memory or buffer space to provision the new socket. 
		 * 🔒 Permission Constraints
		 * EACCES / WSAEACCES: The process lacks the required privilege to create a socket of this specific type or protocol. (For example: creating raw sockets SOCK_RAW usually requires root/administrative permissions)
		*/
		throw ServerException();
	}

	// allow immediate restart after server shutdown.
	#ifdef __APPLE__
	int no_sigpipe = 1;
	setsockopt(this->_socketFd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
	#endif

	int opt = 1;
	setsockopt(this->_socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	//config file contents goes here?
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	// port number need to be replaced according to config 
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(this->_socketFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1)
	{
		const int errorNumber = errno;
		if (errorNumber == EACCES)
		{
			Logger::systemError(Logger::FATAL, "bind: permission denied", errorNumber);
		}
		else if (errorNumber == EADDRINUSE)
		{
			Logger::systemError(Logger::FATAL, "bind: address already in use", errorNumber);
		}
		else
		{
			Logger::systemError(Logger::FATAL, "bind failed", errorNumber);
		}
		/** OTHER ERRORs : probably won't get. only log if so.
		 * ❌ EADDRNOTAVAIL / WSAEADDRNOTAVAIL (Cannot assign requested address)
		 * The Cause: You tried to bind the socket to a specific IP address that does not physically belong to any network interface on the local machine (e.g., trying to bind to a public web IP instead of your internal LAN IP).
		 * The Fix: If you want your program to listen on all available network cards, bind using the wildcard address INADDR_ANY (0.0.0.0).
		 * ⚠️ EBADF / WSAENOTSOCK (Bad descriptor)
		 * The Cause: The socket file descriptor passed into bind() is completely invalid. This usually happens if your previous socket() initialization failed (returned -1) and you forgot to check it before calling bind().
		 */
		throw ServerException();
	}
	
	if (listen(this->_socketFd, 5) == -1)
	{
		const int errorNumber = errno;
		Logger::systemError(Logger::FATAL, "listen failed", errorNumber);
		throw ServerException();
	}

	this->_isRunning = true;
	Logger::info("listening socket established");
}

// do we need to return as int?
int Server::start(void)
{
	try
	{
		setsocket(); // error happen will throw exception
		setPollFds();
		runningLoop();
	}
	catch(const std::exception& e)
	{
		return 1;
	}
	return 0;
}

const char* Server::ServerException::what() const noexcept
{
	return "error";
}