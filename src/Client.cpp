#include "Client.hpp"

Client::Client(int fd, size_t configIndex)
	: _fd(fd),
	  _configIndex(configIndex),
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

size_t	Client::getConfigIndex(void) const
{
	return _configIndex;
}

bool	Client::getIsConnected(void)
{
	return _isConnected;
}

void	Client::disConnected(void)
{
	_isConnected = false;
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

void Client::prepareResponse(const ServerConfig& serverConfig)
{
	//status is not 0
	if(_request.httpStatus != REQ_OK)
	{
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus));
	}
	else
		_response = _builder.buildResponse(_request, serverConfig);
	_writeBuffer = _builder.serialize(_response);
}