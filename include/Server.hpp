#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <iostream>
// error macro is here
#include <fstream>

// for socket;
#include <netinet/in.h> 
#include <sys/socket.h>

// for poll
#include <poll.h>

// for vector
#include <vector>

class Server
{
	public:
		Server(void);
		~Server(void);

		int start();

		bool	getRunning(void);
		int		getSocketFd(void);

		// system methods
		void	setsocket(void);
		void	setPollFds(void);
		void	runningLoop(void);

		// methods for loop
		void	acceptNewClient(void);
		bool	receiveClientData(int fd);
		void	markForClose(int fd);
		bool	sendClientData(int fd);

		class ServerException : public std::exception
		{
			const char* what() const throw();
		};
	
	private:
		int							_socketFd;
		bool						_isRunning;
		std::vector<struct pollfd>	_pollfds;
		// map container of fd and client connection
		

};

#endif
