#include "webserv.hpp"

#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

webserv::webserv(){}

webserv::~webserv(){}

int	webserv::init()
{
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);

	if (_serverFd < 0)
	{
		std::cerr << "socked failed\n";
		return FAILURE;
	}

	struct	sockaddr_in	address = {};

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);
	
	if (bind(
			_serverFd,
			reinterpret_cast<struct sockaddr *>(&address),
			sizeof(address)) < 0)
	{
		std::cerr << "bind failed\n";
		return FAILURE;
	}

	if (listen(_serverFd, 10) < 0)
	{
		std::cerr << "listen failed\n";
		return FAILURE;
	}

	std::cout << "Server listening on port 8080\n";
	
	return SUCCESS;
}

int	webserv::run()
{
	std::cout << "Server is running... " << std::endl;

	int	client_fd = accept(_serverFd, NULL, NULL);
	if (client_fd < 0)
	{
		std::cerr << "accept failed\n";
		return FAILURE;
	}

	std::cout << "Client connected\n";

	char	buffer[1024];

	ssize_t	bytesRead = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

	if (bytesRead <= 0)
	{
		std::cerr << "recv failed\n";
		close(client_fd);
		return FAILURE; 
	}

	buffer[bytesRead] = '\0';

	std::cout << "Received request:\n" << buffer << std::endl;

	std::string	response = 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: 20\r\n"
		"Connection: close\r\n"
		"\r\n"
		"<h1>Hello World</h1>";
	
	send(client_fd, response.c_str(), response.size(), 0);

	close(client_fd);

	return SUCCESS;
}