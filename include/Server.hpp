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

struct Connection
{
	std::string inputBuffer;
	std::string outputBuffer;
};
class Server
{
	private:
		int _port;
		int	_serverFd;
		std::map<int, Connection> _connections;
		void createSocket();
		void bindSocket();
		void listenSocket();
		int acceptClient();
		void receiveRequest(int clientFd);
		void sendResponse(int clientFd);
	public:
		Server(int _port);
		~Server();
		void start();

};

#endif
