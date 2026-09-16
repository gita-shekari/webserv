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
#include "Logger.hpp"

class Server
{
	public:
		Server(const std::vector<ServerConfig>& config);
		~Server(void);

		int start();

		bool	getRunning(void);

		// system methods
		void	addPollFds(int fd, short events);
		void	runningLoop(void);
		bool	setNonBlocking(int fd);

		// for listen fd
		int		createListeningSocket(const ServerConfig& config);
		void	setListeningSockets(void);
		bool	isListeningFd(int fd) const;

		// methods for loop
		void			acceptNewClient(int listenerFd);
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
		std::map<int, size_t>						_listeners;
		bool										_isRunning;
		std::vector<struct pollfd>					_pollfds;
		std::map<int, Client>						_clients;
		const std::vector<struct ServerConfig>& 	_configs;

		Server(void);
};
