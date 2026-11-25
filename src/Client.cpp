#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

Client::Client(int fd, size_t serverIndex)
	: _fd(fd),
	  _serverIndex(serverIndex),
	  _writeOffSet(0),
	  _lastActivity(std::chrono::steady_clock::now())
{
	std::cout << "A client is created fd: " << fd << std::endl;
}

Client::~Client(void)
{
	std::cout << "fd " << _fd << " is destroyed" << std::endl;
}

int		Client::getFd(void) const
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

bool	Client::getIsMarkedForClose() const
{
	return _isMarkedForClose;
}

const std::string& Client::getWriteBuffer() const
{
	return _writeBuffer;
}

Request	Client::getReq(void) const
{
	return _request;
}
ssize_t Client::getWriteOffSet() const
{
	return _writeOffSet;
}

ClientState Client::getState() const
{
	return _state;
}
void Client::setState(ClientState st)
{
	_state = st;
}
void Client::setWriteOffSet(ssize_t byte)
{
	_writeOffSet = byte;
}

std::chrono::steady_clock::time_point	Client::getLastActivity(void) const
{
	return _lastActivity;
}


void	Client::setEffectiveMaxBodySize(size_t size)
{
	_request.effectiveMaxBodySize = size;
}

void	Client::setIsMarkedForClose(void)
{
	_isMarkedForClose = true;
}

bool Client::getPeerReadClosed() const
{
    return _peerReadClosed;
}

void Client::setPeerReadClosed()
{
    _peerReadClosed = true;
}

void Client::setCloseAfterResponse()
{
	_closeAfterResponse = true;
}

void	Client::appendReadBuffer(const char* data, size_t size)
{
	_readBuffer.append(data, size);
}
ParseStatus	Client::parseReadBuffer(void)
{
	return _parser.parseRequest(_readBuffer, _request);
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
void Client::prepareResponseForSending()
{
	_writeBuffer = _builder.serialize(_response);
	_writeOffSet = 0;
	_state = SENDING_RESPONSE;
}

void	Client::updateLastActivity(void)
{
	_lastActivity = std::chrono::steady_clock::now();
}

bool needToClose(const std::string& value)
{
	size_t comma = 0;
	size_t start = 0;
	std::string token;
	while (start < value.size())
	{
		comma = value.find(',', start);
		if (comma == std::string::npos)
			comma = value.size();

		token = Utils::toLower(Utils::trim(value.substr(start, comma - start)));
		if (token == "close")
			return true;
		start = comma + 1;
	}
	return false;
}

bool	Client::resetClient()
{
	if (_closeAfterResponse)
		return false;

	std::map<std::string, std::string>::const_iterator it = _request.headers.find("connection");
	if (it != _request.headers.end() && needToClose(it->second))
		return false;

	_readBuffer.erase(0, _parser.getConsumedBytes());
	_writeBuffer.clear();
	_writeOffSet = 0;
	_locationIndex = static_cast<size_t>(-1);
	_request.clear();
	_parser.reset();
	// no? _builder.clear()
	_state = READ_REQ;
	return true;
}
