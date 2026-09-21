#include "Client.hpp"

Client::Client(int fd, size_t configIndex)
	: _fd(fd),
	  _configIndex(configIndex),
	  _isConnected(true),
	  _lastActivity(std::chrono::steady_clock::now())
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

std::string Client::getWriteBuffer()
{
	return _writeBuffer;
}

Request	Client::getReq(void)
{
	return _request;
}

std::chrono::steady_clock::time_point	Client::getLastActivity(void) const
{
	return _lastActivity;
}

void	Client::disConnected(void)
{
	_isConnected = false;
}
ParseStatus	Client::parseRequest(char *buffer, const ServerConfig& serverConfig)
{
	_readBuffer.append(buffer);

	ParseStatus status = _parser.parse(_readBuffer, _request, serverConfig.client_max_body_size);
	if (status == COMPLETE)
	{
		_readBuffer.clear();
	}
	return status;
}
void Client::prepareResponse(const ServerConfig& serverConfig)
{
	//status is not 0
	if(_request.httpStatus != REQ_OK)
	{
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
	}
	else
		_response = _builder.buildResponse(_request, serverConfig);
	_writeBuffer = _builder.serialize(_response);
}

void	Client::updateLastActivity(void)
{
	_lastActivity = std::chrono::steady_clock::now();
}
