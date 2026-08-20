#include "Client.hpp"

Client::Client(int fd)
	: _fd(fd),
	  _isConnected(true)
{
	std::cout << "A client is created fd: " << fd << std::endl;
}

Client::~Client(void)
{
	std::cout << "fd " << _fd << " is destroyed" << std::endl;
}

int		Client::getFd(void)
{
	return _fd;
}

bool	Client::getIsConnected(void)
{
	return _isConnected;
}

void	Client::disConnected(void)
{
	_isConnected = false;
}

void	Client::setFd(void)
{
	_fd = -1;
}

void	Client::appendReadBuffer(char *buffer)
{
	// any error might happend?
	if (_isConnected)
		_readBuffer.append(buffer); // also need to clean up readBuffer later.

	std::cout << "in client " << _fd << ": " << std::endl;
	std::cout << "read buffer is: " << _readBuffer << std::endl;
}