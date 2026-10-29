#pragma once

#include <string>
#include <iostream>
// error macro is here
#include <fstream>

// for socket;
#include <netinet/in.h>
#include <sys/socket.h>
// for closing socket
#include <unistd.h>

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
		~Server();
	
		int		start();

		class ServerException : public std::exception
		{
			const char* what() const noexcept override;
		};

		//todo: no use
		bool	getRunning(void);

	private:
		static const int	CLIENT_TIMEOUT_SEC = 30;
		static const int	POLL_TIMEOUT_MS = 1000;
		static const size_t	MAX_ACCEPTS_PER_EVENT = 32;

		bool										_isRunning;
		const std::vector<struct ServerConfig>& 	_config;
		std::vector<struct pollfd>					_pollfds;

		std::map<int, size_t>						_listeners;
		std::map<int, Client>						_clients;
		std::map<int, int>							_pipeFds;

		//prevent copying
		Server(void);
		Server(const Server&);
		Server& operator=(const Server&);

		//1. server lifecycle / event loop
		void	runningLoop();
		void 	handlePollCallError(int& eagainCount);
		bool	handlePollException(int fd, short revents);
		bool	setNonBlocking(int fd);
		void	addPollFds(int fd, short events);
		void	checkTimeouts(void);

		//2. listening sockets
		void	validateUniquePorts(void) const;
		void	setListeningSockets(void);
		int		createListeningSocket(const ServerConfig& config);
		bool	isListeningFd(int fd) const;

		//3. clients
		void	acceptNewClient(int listenerFd);
		bool	receiveClientData(Client& client);
		bool	sendClientData(Client& client);
		bool	isClientFd(int fd) const;
		Client& getClient(int clientFd);

		//4. event handling / CGI
		bool	isCgiPipeFd(int fd);
		Client& getBelongedClient(int fd);
		void	handleEvent(pollfd& pfd, short& revents, Client& client);
		void	handleCgiPipeEvent(pollfd& pfd, short& revents);

		//5. resource cleanup
		void	markForClose(int fd);
		void	removeMarkedClients();
		void	shutdown();
		void	removePollFd(int fd);
		void	closeClient(int clientFd);
		void	cleanupCgi(int clientFd);
};
