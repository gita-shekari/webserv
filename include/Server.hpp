#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include <map>
#include <sys/socket.h>
#include <cstring>
#include <stdexcept>
#include <netinet/in.h>
#include <string>
#include <unistd.h>

class Server
{
	private:
		int _port;
		int	_serverFd;
		std::map<int, Client> _clients;
		void createSocket();
		void bindSocket();
		void listenSocket();
		void acceptClient();
	public:
		Server(int _port);
		~Server();
		void start();

};

#endif
