#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

Client::Client(int fd, size_t serverIndex)
	: _fd(fd),
	  _serverIndex(serverIndex),
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

size_t	Client::getServerIndex(void) const
{
	return _serverIndex;
}

size_t	Client::getLocationIndex(void) const
{
	return _locationIndex;
}

size_t	Client::getEffectiveMaxBodySize(void) const
{
	return _request.effectiveMaxBodySize;
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

void	Client::setRequestPath(const std::string& str)
{
	_request.path = str;
}

void	Client::setEffectiveMaxBodySize(size_t size)
{
	_request.effectiveMaxBodySize = size;
}

void	Client::disConnected(void)
{
	_isConnected = false;
}

void	Client::appendReadBuffer(const char* data, size_t size)
{
	_readBuffer.append(data, size);
}
ParseStatus	Client::parseReadBuffer(void)
{
	ParseStatus status = _parser.parseRequest(_readBuffer, _request);
	if (status == COMPLETE)
		_readBuffer.clear();
	return status;
}

void	Client::matchLocation(const std::vector<LocationConfig>& locations)
{
	size_t	longestMatch = 0;
	//TODO: make sure to check if there is no matching location
	for (size_t i = 0; i < locations.size(); ++i)
	{
		const std::string& locationPath = locations[i].path;
		size_t	length = locationPath.size();

		if (length > _request.path.size())
			continue;

		if (_request.path.compare(0, length, locationPath) != 0)
			continue;

		bool validBoundary = (length == _request.path.size())
							|| (locationPath == "/")
							|| (_request.path[length] == '/');
		if (!validBoundary)
			continue;

		if (length > longestMatch)
		{
			longestMatch = length;
			_locationIndex = i;
		}
	}
}

//enum	ConfigBehavior
//{
//	REDIRECT,
//	CGI,
//	UPLOAD,
//	DELETE,
//	DIRECTORU_LISTING,
//	STATIC
//};

// ConfigBehavior checkConfigBehavior(const ServerConfig& sc, const LocationConfig& lc, const Request& req)
// {
// 	std::string root = sc.root;
// 	if (lc.root.size() != 0)
// 		root = lc.root;
// 	root + lc.path;
// 	if (lc.cgiHandlers.size() != 0)
// }

// void Client::routing(const ServerConfig& serverConfig)
// {
// 	if(_request.httpStatus != REQ_OK)
// 	{
// 		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
// 	}
// 	else
// 	{
// 		ConfigBehavior action = checkConfigBehavior(serverConfig, serverConfig.locations[_locationIndex], _request);
// 	}
// 	//else
// 	//	_response = _builder.buildResponse(_request, serverConfig);
// 	//_writeBuffer = _builder.serialize(_response);
// }

void	Client::updateLastActivity(void)
{
	_lastActivity = std::chrono::steady_clock::now();
}
