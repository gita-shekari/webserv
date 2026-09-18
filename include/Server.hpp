#pragma once
# include "Client.hpp"
# include "Config.hpp"
# include "Logger.hpp"

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
		void	addPollFds(int fd, short events);
		bool	setNonBlocking(int fd);

		// for listen fd
		void	setListeningSockets(void);
		int		createListeningSocket(const ServerConfig& config);
		bool	isListeningFd(int fd) const;

		// methods for loop
		void	acceptNewClient(int listenerFd);
		template <typename ClientsIt>
		bool	receiveClientData(int fd, ClientsIt it);
		void	markForClose(int fd);
		bool	sendClientData(int fd);
		void	removeCloseClient();

		class ServerException : public std::exception
		{
			const char* what() const noexcept override;
		};
	
	private:
		std::map<int, size_t>						_listeners;
		bool										_isRunning;
		std::vector<struct pollfd>					_pollfds;
		std::map<int, Client>						_clients;
		const std::vector<struct ServerConfig>& 	_config;

		Server(void);
};
