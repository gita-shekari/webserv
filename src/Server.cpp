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

void Server::start(void)
{
	this->_socketFd = socket(AF_INET, SOCK_STREAM, 0);
	//AF_INET : IPv4 protocol SOCK_STREAM: TCP socket
	// this is for linux, do we need to write a seperate one for windows? how to decide which one to use?

	if (this->_socketFd == -1)
	{
		std::cerr << "Server failed at set up socket." << std::endl;
		throw ServerException();
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
	serverAddress.sin_port = htons(8082);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(this->_socketFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1)
	{
		std::cerr << "Server failed at binding socket." << std::endl;
		throw ServerException();
	}
	
	if (listen(this->_socketFd, 5) == -1)
	{
		std::cerr << "Server failed at setting socket listening." << std::endl;
		throw ServerException();
	}
	this->_isRunning = true;
	std::cout << "socket is established" << std::endl;
}

const char* Server::ServerException::what() const throw()
{
	return "error";
}