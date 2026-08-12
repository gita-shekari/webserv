#include "Server.hpp"

Server::Server(int port): _port(port), _serverFd(-1)
{
}
Server::~Server()
{

}
/**
 * @brief calling socket function with this inputs and if it was successful, returns a fd
    AF_INET,       ← I want IPv4
    SOCK_STREAM,   ← I want stream communication (TCP)
    0              ← use the default protocol for this combination
)
 *
 */
void Server::start()
{
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if(_serverFd == -1)
		throw std::runtime_error("failed to create socket");
	std::cout << "Server socket fd: " << _serverFd << std::endl;
	std::cout << "Server port is " << _port << std::endl;
}

// void Server::addClient(int fd, Client client)
// {

// }
