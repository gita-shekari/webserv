#pragma once

#include <string>
#include <iostream>
// error macro is here
#include <fstream>

// for socket;
#include <netinet/in.h> 
#include <sys/socket.h>

// for poll
#include <poll.h>

// for containers
#include <vector>
#include <map>

#include "Client.hpp"
#include "Config.hpp"

class Server
{
	public:

		Server(struct ServerConfig& config);
		~Server(void);

		int start();

		bool	getRunning(void);
		int		getSocketFd(void);

		// system methods
		void	setsocket(void);
		void	setPollFds(void);
		void	runningLoop(void);

		// methods for loop
		void			acceptNewClient(void);
		template <typename ClientsIt>
		ReceiveStatus	receiveClientData(int fd, ClientsIt it);
		void			markForClose(int fd);
		bool			sendClientData(int fd);
		void			removeCloseClient();

		class ServerException : public std::exception
		{
			const char* what() const noexcept override;
		};
	
	private:
		int							_socketFd;
		bool						_isRunning;
		std::vector<struct pollfd>	_pollfds;
		std::map<int, Client>		_clients;
		struct ServerConfig& 		_config;
		// map container of fd and client connection

		Server(void);
};

void logError(const char* func, int errNb);