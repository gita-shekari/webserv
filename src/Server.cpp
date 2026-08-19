#include "Server.hpp"
// for socket;
#include <netinet/in.h> 
#include <sys/socket.h>
#include <unistd.h> // for closing socket

Server::Server(void)
	: _socketFd(-1), 
	  _isRunning(false)
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
	close(fd);
	for (size_t i =0; i < _pollfds.size(); i++)
	{
		if (_pollfds[i].fd == fd)
		{
			_pollfds.erase(_pollfds.begin() + i); // no other way???
			break;
		}
	}
	std::cout << "Mark this fd is finished and ready to close. " << fd << std::endl;
	std::cout << "We need a stucture to record fd and buffers" << std::endl;
	std::cout << "----remain-----" << std::endl;
	for (size_t i = 0; i < _pollfds.size(); i++)
	{
		std::cout << "fd: " << _pollfds[i].fd << std::endl; 
	}
}

void	Server::acceptNewClient(void)
{
	//struct sockaddr_in clientAddr;
	//socklen_t len = sizeof(clientAddr);
	
	int clientFd = accept(_socketFd, NULL, NULL);
	if (clientFd == -1)
	{
		// do we need to do something?
		std::cerr << "Error at accepting client" << std::endl;
		return ;
	}

	struct pollfd client;
	client.fd = clientFd;
	client.events = POLLIN;
	client.revents = 0;
	_pollfds.push_back(client);

	// create instance of client/connection class.

	//something related to connections
	std::cout << "A new client connected." << std::endl;
}

bool	Server::receiveClientData(int fd)
{
	char buffer[1024] = {0};

	ssize_t bytesReceived = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (bytesReceived > 0)
	{
		std::cout << "Receiving from client: " << buffer << std::endl;
		std::cout << "fd: " << fd << std::endl;
		// 1. append to the corresponding fd buffers.
		// 2. parse to HTTP request -> if complete,
									// run the request and get response; return true
									// else return false;
		return false;
	}
	else if (bytesReceived == 0)
	{
		std::cout << "Client has disconnected before sending all the data";
		markForClose(fd);
	}
	else
	{
		std::cerr << "Socket error during recv." << std::endl;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			markForClose(fd);
		}
	}
	return false;
}

bool	Server::sendClientData(int fd)
{
	std::cout << "the client request has been processed. The response is ready and translate TCP. " 
			  << "\n here we prepare for sending the data back. fd is " << fd << std::endl;

	// also need to remove from pollfds and another things. 
	if (fd < 0) // if finish sending the data, then return true.
		return true;
	else
		return false;
}

void	Server::runningLoop(void)
{
	while (_isRunning)
	{
		int res = poll(&_pollfds[0], _pollfds.size(), -1); //timeout?

		if (res == 0)
		{
			// timeout, no event happend;
			continue;
		}
		else if (res < 0)
		{
			if (errno == EINTR)
			{
				// signal is interupted. start over 
				continue;
			}
			break;
				// other errors;
		}

		for (size_t i = 0; i < _pollfds.size(); i++)
		{
			short revents = _pollfds[i].revents;
			
			if (revents == 0)
			{
				//checkTimeouts();
				continue;
			}
				
			int fd = _pollfds[i].fd;

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
					if (receiveClientData(fd))
					{
						_pollfds[i].events |= POLLOUT;
					}
				}

				if (revents & POLLOUT)
				{
					if (sendClientData(fd))
					{
						_pollfds[i].events &= ~POLLOUT;
					}
				}
				
				if (revents & (POLLERR | POLLNVAL))
				{
					markForClose(fd);
				}
			}
			//removeCloseClient();
			//checkTimeouts();
		}
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
		std::cerr << "Server failed at set up socket." << std::endl;
		throw ServerException();
		return ;
	}

	// allow immediate restart after server shutdown.
	#ifdef __APPLE__
	int no_sigpipe = 1;
	setsockopt(this->_socketFd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
	#endif

	int opt = 1;
	setsockopt(this->_socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// maybe config file contents goes here?
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(this->_socketFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1)
	{
		std::cerr << "Server failed at binding socket." << std::endl;
		throw ServerException();
		return ;
	}
	
	if (listen(this->_socketFd, 5) == -1)
	{
		std::cerr << "Server failed at setting socket listening." << std::endl;
		throw ServerException();
		return ;
	}

	this->_isRunning = true;
	std::cout << "socket is established" << std::endl;
}

int Server::start(void)
{
	try
	{
		setsocket();
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

const char* Server::ServerException::what() const throw()
{
	return "error";
}