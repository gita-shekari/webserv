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

Request	Client::getReq(void)
{
	return _request;
}

ParseStatus	Client::parseRequest(char *buffer)
{
	_readBuffer.append(buffer);

	ParseStatus status = _parser.parse(_readBuffer, _request);
	if (status == COMPLETE)
	{
		_readBuffer.clear();
	}
	return status;
}
std::string Client::getWriteBuffer()
{
	return _writeBuffer;
}
void Client::buildResponse(const ServerConfig& serverConfig)
{
	_response = _builder.build(_request, serverConfig);
	_writeBuffer = _builder.serialize(_response);
}
