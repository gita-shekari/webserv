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

Request	Client::getReq(void)
{
	return _request;
}

ParseStatus	Client::parseRequest(char *buffer, const ServerConfig& serverConfig)
{
	_readBuffer.append(buffer);

	ParseStatus status = _parser.parse(_readBuffer, _request, serverConfig.clientMaxBodySize);
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
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
	}
	else
		_response = _builder.buildResponse(_request, serverConfig);
	_writeBuffer = _builder.serialize(_response);
}
