#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include <map>
#include <sys/socket.h>
#include <stdexcept>

class Server
{
	private:
		int _port;
		int	_serverFd;
		std::map<int, Client> _clients;
	public:
		Server(int _port);
		~Server();
		void start();

};

#endif
