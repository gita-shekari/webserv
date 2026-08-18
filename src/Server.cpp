#include "Server.hpp"


Server::Server(int port): _port(port), _serverFd(-1)
{
}
Server::~Server()
{
	 if (_serverFd != -1)
		close(_serverFd);
}
/**
	@brief calling socket function with this inputs and if it was successful, returns a fd
	AF_INET,       ← I want IPv4
	SOCK_STREAM,   ← I want stream communication (TCP)
	0              ← use the default protocol for this combination
 */
void Server::createSocket()
{
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if(_serverFd == -1)
		throw std::runtime_error("failed to create socket");
	std::cout << "Server socket fd: " << _serverFd << std::endl;
	std::cout << "Server port is " << _port << std::endl;
}

void Server::bindSocket()
{
	struct sockaddr_in address;
	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(_port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(_serverFd, reinterpret_cast<struct sockaddr *>(&address) , sizeof(address)) == -1)
		throw std::runtime_error("failed to bind");
}

void Server::listenSocket()
{
	if(listen(_serverFd, 10) == -1)
		throw std::runtime_error("failed to listen");
}
void Server::acceptClient()
{

	
}
/**
 * @brief starting server by calling two functions to create a socket and bind it
 */
void Server::start()
{
	createSocket();
	bindSocket();
	listenSocket();
	acceptClient();

}

// void Server::addClient(int fd, Client client)
// {

// }
