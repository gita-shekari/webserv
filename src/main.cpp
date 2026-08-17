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
	client.events = POLLIN | POLLOUT;
	client.revents = 0;
	pollfds.push_back(client);

	//something related to connections
	std::cout << "A new client connected." << std::endl;
}

void	receiveClientData(int fd)
{
	char buffer[1024] = {0};

	ssize_t bytesReceived = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (bytesReceived > 0)
	{
		std::cout << "Receiving from client: " << buffer << std::endl;
		// 1. append to the corresponding fd buffers.
		// 2. parse to HTTP request -> if complete, run the request and get response;
	}
	else if (bytesReceived == 0)
	{
		std::cout << "Client has disconnected before sending all the data";
		// markForClose(fd);
	}
	else
	{
		std::cerr << "Socket error during recv." << std::endl;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			// markeForClose(fd);
		}
	}	
}

void	sendClientData(int fd)
{
	std::cout << "the client request has been processed. The response is ready and translate TCP. " 
			  << "\n here we prepare for sending the data back. fd is " << fd << std::endl;
}

void	markForClose(int fd)
{
	std::cout << "Mark this fd is finished and ready to close. " << fd << std::endl;
	std::cout << "We need a stucture to record fd and buffers" << std::endl;
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
		pollfds[0].fd = server.getSocketFd();
		pollfds[0].events = POLLIN;
		pollfds[0].revents = 0;
		// while server is running.
		while (server.getRunning())
		{
			int res = poll(&pollfds[0], pollfds.size(), 1000); //timeout?

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
					if (revents == POLLIN)
					{
						accpetNewClient(server, pollfds);
					}
				}
				else
				{
					if (revents == POLLIN)
					{
						receiveClientData(fd);
					}

					if (revents == POLLOUT)
					{
						sendClientData(fd);
					}
					
					if (revents == POLLERR || revents == POLLNVAL)
					{
						markForClose(fd);
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