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
// helper
std::string& getExtension(const std::string& reqPath)
{
	size_t idx = reqPath.size() - 1;
	std::string tmp;
	while (idx >= 0)
	{
		if (reqPath[idx] == '.')
			break;
		tmp += reqPath[idx];
		idx--;
	}
	// TODO: should I make sure idx != 0? because we are sure it won't happen. 
	std::string res = std::string(tmp.begin(), tmp.end());
	return res;
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

void Client::handleDirectory(const ServerConfig& serverConfig, std::string& fullPath)
{
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	// if req method is GET
	if (toLower(_request.method) == "get")
	{
		if (lc.index.empty() && !lc.autoindex)
		{
			_request.httpStatus = FORBIDDEN;
			_builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
			return ;
		}
		else if (lc.index.empty()) // handleDirectoryListing
		{
			DIR *dir = opendir(fullPath.c_str());
			struct dirent *ent;
			if (dir != NULL)
			{
				// readdir(dir)
				// closedir(dir);
			}
			else
			{
				//error with open directory
			}
		}
		else
		{
			std::string newPath = fullPath + lc.index;
			handleRegularFile(serverConfig, newPath);
		}

	}

	// if req method is POST
	if (toLower(_request.method) == "post")
	{

	}
	// if req method is DELETE
	if (toLower(_request.method) == "delete")
	{

	}
}

void	Client::handleRegularFile(const ServerConfig& serverConfig, std::string& path)
{
	std::string& extension = getExtension(path);
	if (extension == "py" || extension == "php")
	{
		return ; //handleCGI(serverConfig, extension);
	}
	else
	{
		return ; // handleStatic(serverConfig);
	}
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
		// TODO: make sure request.path will never be empty()
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] == '/')
		{
			return handleDirectory(serverConfig, filesystemPath);// handle directory
		}
		else
		{
			return ; //handleRedirect(301, _request.path + '/', serverConfig)// handle redirect and add / to the end;
		}
	}
	if (S_ISREG(buf.st_mode))
	{
		return handleRegularFile(serverConfig, _request.path);
	}

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
