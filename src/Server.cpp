#include "Server.hpp"
// for socket;
#include <netinet/in.h> 
#include <sys/socket.h>
#include <unistd.h> // for closing socket
#include <vector>
#include <algorithm>

Server::Server(struct ServerConfig& config)
	: _socketFd(-1), 
	  _isRunning(false),
	  _config(config)
{
	std::cout << "Server is up" << std::endl;
}

Server::~Server(void)
{
	if (this->_socketFd != -1)
		close(this->_socketFd);
	std::cout << "Server is down now" << std::endl;
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
	std::cout << "Mark this fd of Client to false, ready to close. " << fd << std::endl;
	// shouldn't do this because it might create a client if it doent exist;
	//_clients[fd].Client::disConnected();

	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it != _clients.end())
	{
		it->second.disConnected();
	}
}

void	Server::acceptNewClient(void)
{
	//struct sockaddr_in clientAddr;
	//socklen_t len = sizeof(clientAddr);
	
	int clientFd = accept(_socketFd, NULL, NULL);
	if (clientFd == -1)
	{
		// if we set socket as O_NONBLOCK, then no connection is ready can return -1
		if (errno == EINTR)
			logError("[WARNING] accept signal interrupt", errno); 
		else if (errno == EMFILE || errno == ENFILE)
			logError("[FATAL] accept, running out of file descriptors. ", errno);
		else if (errno == EBADF || errno == ENOTSOCK)
			logError("[DEBUG] invalid socketFd at accept.", errno); // for debugging. we won't have this error.
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

	std::cout << "A new client connected." << std::endl;
}

template <typename ClientsIt>
ReceiveStatus	Server::receiveClientData(int fd, ClientsIt it)
{
	char buffer[1024] = {0};

	ssize_t bytesReceived = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (bytesReceived > 0)
	{
		std::cout << "Receiving from client: " << buffer << std::endl;
		std::cout << "fd: " << fd << std::endl;

		
		ParseStatus status = it->second.parseRequest(buffer);
		if (status == COMPLETE)
			return COMPLETE;
		else if (status == INCOMPLETE)
			return 	CONTINUE;
		else if (status == ERROR)
			return ERROR;

	}
	else if (bytesReceived == 0)
	{
		std::cout << "Client has disconnected before sending all the data";
		markForClose(fd);
		return CONTINUE;
	}
	else
	{
		std::cerr << "Socket error during recv." << std::endl;
		logError("recv", errno); // info
		if (errno == ECONNRESET || errno == ETIMEDOUT)
		{
			markForClose(fd);
			std::cerr << "[WARNING] recv connection is broken " << strerror(errno) << std::endl;
		}
		else if (errno == EINTR)
		{
			std::cerr << "[WARNING] recv is being interrupted " << strerror(errno) << std::endl;
		}
		return CONTINUE;
	}
}

bool	Server::sendClientData(int fd)
{
	std::cout << "the client request has been processed. The response is ready and translate TCP. " 
			  << "\n here we prepare for sending the data back. fd is " << fd << std::endl;

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
			if (errno == EINTR)
			{
				std::cerr << "[WARNING] poll failed due to " << strerror(errno) << std::endl;
				continue;
			}
			else if (errno == EAGAIN || errno == ENOMEM)
			{
				if (eagainCount > 2)
				{
					std::cerr << "[FATAL] poll failed due to kernel temporarily out of resources.\n" 
							  << "Retried three times. Now exit. " << std::endl; 
					throw ServerException();
				}
				else
				{
					eagainCount++;
					std::cerr << "[FATAL] poll failed due to kernel temporarily out of resources.\n" 
							  << "Try poll() again..." << std::endl;
					continue;
				}
			}
			else
			{
				std::cerr << "[FATAL] poll failed due to " << strerror(errno) 
							<< " (errno = " << errno << ")" << std::endl; 
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
			if (revents & POLLNVAL) // fd is closed. our program wont have it. debug
			{
				std::cerr << "[DEBUG] POLLENVAL fd is invalid. Logic bug, prevent double close." << std::endl;
				continue;
			}
			if (revents & (POLLERR | POLLHUP))
			{
				std::cerr << "[INFO] fd is closed" << std::endl;
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
						std::cerr << "[DEBUG] check pullfd but client does not exit. " << std::endl;
						_clients[fd] = Client(fd);
						continue;
					}

					ReceiveStatus status = receiveClientData(fd, it);
					if (status == COMPLETE)
					{
						_pollfds[i].events |= POLLOUT;
						// runScript() or CGI;
					}
					if (status == ERROR)
					{
						// check errtype and build response
						// it->second.getReq().errtype ： the enum has a number as status code, can be used directly to response. 

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
		std::cerr << "[FATAL] Socket cannot be created due to " << strerror(errno)
					  << " (errno = " << errno << " )" << std::endl;
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
		if (errno == EACCES)
		{
			
			std::cerr << "[FATAL] Permission denied. Ports under 8080 require administrator/root privileges.\n"
					  << "Try running your command with 'sudo'." << std::endl;
		}
		else if (errno == EADDRINUSE)
		{
			std::cerr << "[FATAL] Port is already in use by another application.\n"
                  	  << "Please run 'sudo lsof -i :<port>' to find and terminate the conflicting process.\n";
		}
		else
		{
			std::cerr << "[FATAL] Socket cannot be binded due to " << strerror(errno)
					  << " (errno = " << errno << " )" << std::endl;
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
		std::cerr << "[FATAL] Listen cannot be setup due to " << strerror(errno)
				  << " (errno = " << errno << " )" << std::endl;
		throw ServerException();
	}

	this->_isRunning = true;
	std::cout << "socket is established" << std::endl;
}

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
		std::cerr << e.what() << '\n';
		return 1;
	}
	return 0;
}

const char* Server::ServerException::what() const noexcept
{
	return "error";
}