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

bool	Client::parseRequest(char *buffer)
{
	// any error might happend?
	if (!_isConnected)
	{
		std::cout << " in Client::appendReadBuffer: check the connection of client " << _fd << " is offline. so returned." << std::endl;
		return false;
	}
	
	_readBuffer.append(buffer);

	ParseStatus status = _parser.parse(_readBuffer, _request);
	if (status == COMPLETE)
	{
		_readBuffer.clear();
		return true;
	}
	if (status == ERROR)
	{
		// what should we do? print error?
		return true;
	}
	if (status == INCOMPLETE)
		return false;
	return false;
}