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

		int		start(void);

		//getters
		bool	getRunning(void);

		// system methods
		void	runningLoop(void);
		void 	handlePollCallError(int& eagainCount);
		bool	handlePollException(int fd, short revents);
		void	addPollFds(int fd, short events);
		bool	setNonBlocking(int fd);

		// for listen fd
		void	validateUniquePorts(void) const;
		void	setListeningSockets(void);
		int		createListeningSocket(const ServerConfig& config);
		bool	isListeningFd(int fd) const;

		// for client fd
		void	acceptNewClient(int listenerFd);
		bool	isClientFd(int fd) const;
		template <typename ClientsIt>
		bool	receiveClientData(int fd, ClientsIt it);
		void	markForClose(int fd);
		bool	sendClientData(int fd);
		void	removeCloseClient();

		// for timeout
		void	checkTimeouts(void);

		class ServerException : public std::exception
		{
			const char* what() const noexcept override;
		};
	
	private:
		static const int	CLIENT_TIMEOUT_SEC = 30;
		static const int	POLL_TIMEOUT_MS = 1000;

		bool										_isRunning;
		const std::vector<struct ServerConfig>& 	_config;
		std::vector<struct pollfd>					_pollfds;

		std::map<int, size_t>						_listeners;
		std::map<int, Client>						_clients;
		
		Server(void);
};
