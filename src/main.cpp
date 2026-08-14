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
		// while server is running.
		while (server.getRunning())
		{
			// 1. Setup poll() --> ??Should we use epoll or poll;
			// 2. Check pollFds

			struct sockaddr_in clientAddr;
			socklen_t len = sizeof(clientAddr);
			
			int clientFd = accept(server.getSocketFd(), (struct sockaddr*)&clientAddr, &len);
			if (clientFd == -1)
			{
				std::cerr << "Error at accepting client" << std::endl;
				continue;
			}

			std::cout << "A new client connected." << std::endl;

			char buffer[1024] = {0};

			ssize_t bytesReceived = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

			if (bytesReceived > 0)
			{
				std::cout << "Receiving from client: " << buffer << std::endl;
				// do sth?
			}
			else if (bytesReceived == 0)
			{
				std::cout << "Client has disconnected before sending all the data";
			}
			else
			{
				std::cerr << "Socket error during recv." << std::endl;
			}

		}
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
	return 0;
}