#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>

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

size_t	Client::getLocationIndex(void) const
{
	return _locationIndex;
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

//enum	ConfigBehavior
//{
//	REDIRECT,
//	CGI,
//	UPLOAD,
//	DELETE,
//	DIRECTORU_LISTING,
//	STATIC,
//  METHOD_NOT_FOUND
//};

std::string	toLower(const std::string& str)
{
	std::string	ret = str;
	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = std::tolower(static_cast<unsigned char>(ret[i]));
	return ret;
}



ConfigBehavior checkConfigBehavior(const ServerConfig& sc, const LocationConfig& lc, const Request& req)
{
	if (lc.cgiHandlers.size() != 0)
	{
		return CGI;
	}
	else if (lc.upload_store.size() != 0)
	{
		return UPLOAD;
	}
	else if (lc.redirectEnabled)
	{
		return REDIRECT;
	}
	else if ()
}

// this function is a client class util
bool Client::isURIAllowed(const ServerConfig& serverConfig, struct stat* buf, std::string path)
{
	if (stat(path.c_str(), buf) == -1)
	{
		int	saveErr = errno;
		if (saveErr == ENOENT || saveErr == ENOTDIR)
		{
			_request.httpStatus = PAGE_NOT_FOUND;
		}
		else if (saveErr == EACCES)
		{
			_request.httpStatus = FORBIDDEN;
		}
		else
		{
			_request.httpStatus = INTERNAL_SERVER_ERR;
		}
		return false;
	}
	if (!S_ISREG(buf->st_mode) && !S_ISDIR(buf->st_mode))
	{
		_request.httpStatus = FORBIDDEN;
		return false;
	}
	return true;
}

void Client::routing(const ServerConfig& serverConfig)
{

	std::string root = serverConfig.root;
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	if (lc.root.size() != 0)
		root = lc.root;
	std::string filesystemPath = root + _request.path;
	struct stat buf;
	std::vector<std::string>::const_iterator it = std::find(lc.methods.begin(), lc.methods.end(), toLower(_request.method));
	
	if (it == lc.methods.end())
		_request.httpStatus = METHODE_NOT_ALLOWED;
	if(_request.httpStatus != REQ_OK || !isURIAllowed(serverConfig, &buf, filesystemPath))
	{
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return ;
	}
	
	if (S_ISDIR(buf.st_mode))
	{
		// is a directory
	}
	if (S_ISREG(buf.st_mode))
	{
		// regular file
	}

	ConfigBehavior action = checkConfigBehavior(serverConfig, lc, _request);

	//else
	//	_response = _builder.buildResponse(_request, serverConfig);
	//_writeBuffer = _builder.serialize(_response);
}

void	Client::updateLastActivity(void)
{
	_lastActivity = std::chrono::steady_clock::now();
}
