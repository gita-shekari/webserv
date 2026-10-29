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
		// system methods
		Server(const std::vector<ServerConfig>& config);
		~Server(void);
		void		start(void);
		void	runningLoop(void);
		void 	handlePollCallError(int& eagainCount);
		bool	handlePollException(int fd, short revents);
		void	addPollFds(int fd, short events);
		bool	setNonBlocking(int fd);

		bool	isCgiPipeFd(int fd);
		Client& getBelongedClient(int fd);
		void	handleEvent(pollfd& pfd, short& revents, Client& client);
		void	handleCgiPipeEvent(pollfd& pfd, short& revents);

		//Utils
		bool	getRunning(void);
		Client& getClient(int clientFd);
		bool	isListeningFd(int fd) const;
			// for timeout
		void	checkTimeouts(void);
			// Utils for client
		bool	isClientFd(int fd) const;
		void	markForClose(int fd);
		void	removeCloseClient();


		// for listen fd
		void	validateUniquePorts(void) const;
		void	setListeningSockets(void);
		int		createListeningSocket(const ServerConfig& config);

		// for client fd
		void	acceptNewClient(int listenerFd);
		bool	receiveClientData(Client& client);
		bool	sendClientData(Client& client);

		class ServerException : public std::exception
		{
			const char* what() const noexcept override;
		};

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
		Server(void);
};
