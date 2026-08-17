#include <fstream>
#include <iostream>
#include <string>
#include "Config.hpp"
#include "Server.hpp"

// for socket;
#include <netinet/in.h> 
#include <sys/socket.h>

// for poll
#include <poll.h>

// for close
#include <unistd.h>

void	markForClose(int fd, std::vector<struct pollfd>& pollfds)
{
	close(fd);
	for (size_t i =0; i < pollfds.size(); i++)
	{
		if (pollfds[i].fd == fd)
		{
			pollfds.erase(pollfds.begin() + i); // no other way???
			break;
		}
	}
	std::cout << "Mark this fd is finished and ready to close. " << fd << std::endl;
	std::cout << "We need a stucture to record fd and buffers" << std::endl;
	std::cout << "----remain-----" << std::endl;
	for (size_t i = 0; i < pollfds.size(); i++)
	{
		std::cout << "fd: " << pollfds[i].fd << std::endl; 
	}
}

void	accpetNewClient(Server& server, std::vector<struct pollfd>& pollfds)
{
	//struct sockaddr_in clientAddr;
	//socklen_t len = sizeof(clientAddr);
	
	int clientFd = accept(server.getSocketFd(), NULL, NULL);
	if (clientFd == -1)
	{
		std::cerr << "Error at accepting client" << std::endl;
		return ;
	}

	struct pollfd client;
	client.fd = clientFd;
	client.events = POLLIN;
	client.revents = 0;
	pollfds.push_back(client);

	//something related to connections
	std::cout << "A new client connected." << std::endl;
}

bool	receiveClientData(int fd, std::vector<struct pollfd>& pollfds)
{
	char buffer[1024] = {0};

	ssize_t bytesReceived = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (bytesReceived > 0)
	{
		std::cout << "Receiving from client: " << buffer << std::endl;
		// 1. append to the corresponding fd buffers.
		// 2. parse to HTTP request -> if complete,
									// run the request and get response; return true
									// else return false;
		return false;
	}
	else if (bytesReceived == 0)
	{
		std::cout << "Client has disconnected before sending all the data";
		markForClose(fd, pollfds);
	}
	else
	{
		std::cerr << "Socket error during recv." << std::endl;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			markForClose(fd, pollfds);
		}
	}
	return false;
}

bool	sendClientData(int fd)
{
	std::cout << "the client request has been processed. The response is ready and translate TCP. " 
			  << "\n here we prepare for sending the data back. fd is " << fd << std::endl;

	// also need to remove from pollfds and another things. 
	if (fd < 0) // if finish sending the data, then return true.
		return true;
	else
		return false;
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	//// take config file & handle 
	//if (argc != 2)
	//{
	//	// ERROR 
	//	std::cout << "too much arguments" << std::endl;
	//	return 1;
	//}
	//std::ifstream configFile(argv[1]);
	//if (!configFile.is_open())
	//{
	//	// System ERROR
	//	std::cerr << "Error opening the configuration file" << std::endl;
	//	return 1;
	//}
	//std::string configContent;
	//Config* config = new Config();
	//while (std::getline(configFile, configContent))
	//{
	//	if (!config->parse(configContent))
	//	{
	//		configFile.close();
	//		delete config;
	//		return 1;
	//	}
	//}
	//configFile.close();

	// Set up server

	Server server;
	try
	{
		server.start();
		std::vector<struct pollfd> pollfds;
		struct pollfd socket;
		socket.fd = server.getSocketFd();
		socket.events = POLLIN;
		socket.revents = 0;
		pollfds.push_back(socket);
		// while server is running.
		while (server.getRunning())
		{
			int res = poll(&pollfds[0], pollfds.size(), -1); //timeout?

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

			for (size_t i = 0; i < pollfds.size(); i++)
			{
				short revents = pollfds[i].revents;
				
				if (revents == 0)
				{
					//checkTimeouts();
					continue;
				}
					
				int fd = pollfds[i].fd;

				if (fd == server.getSocketFd())
				{
					if (revents & POLLIN)
					{
						accpetNewClient(server, pollfds);
					}
				}
				else
				{
					if (revents & POLLIN)
					{
						if (receiveClientData(fd, pollfds))
						{
							pollfds[i].events |= POLLOUT;
						}
					}

					if (revents & POLLOUT)
					{
						if (sendClientData(fd))
						{
							pollfds[i].events &= ~POLLOUT;
						}
					}
					
					if (revents & (POLLERR | POLLNVAL))
					{
						markForClose(fd, pollfds);
					}
				}
				//removeCloseClient();
				//checkTimeouts();
			}
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
	return 0;
}