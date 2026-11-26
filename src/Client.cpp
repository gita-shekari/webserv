#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
Client::Client(int fd, size_t serverIndex)
	: _fd(fd),
	  _cgiPipesRegistered(false),
	  _serverIndex(serverIndex),
	  _writeOffSet(0),
	  _lastActivity(std::chrono::steady_clock::now())
{
	Logger::debug("client created: fd=" + std::to_string(fd));
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
CGI& Client::getCGI()
{
	return _cgi;
}

const CGI& Client::getCGI() const
{
	return _cgi;
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

void Client::setCgiPipesRegistered(bool value)
{
	_cgiPipesRegistered = value;
}

bool Client::getCgiPipesRegistered() const
{
	return _cgiPipesRegistered;
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
	// CGI currently serves one request per connection, including timeout/error responses.
	if (_cgiPipesRegistered)
		_response.headers["Connection"] = "close";
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

const std::string& Client::getRequestBody() const
{
	return _request.body;
}

void Client::finishCGI(const ServerConfig& serverConfig)
{
	// CGI headers may use CRLF or LF. A missing/oversized header block is a bad gateway response.
	const std::string& output = _cgi.getOutput();
	size_t separator = output.find("\r\n\r\n");
	size_t separatorSize = 4;
	size_t lfSeparator = output.find("\n\n");
	if (lfSeparator != std::string::npos && (separator == std::string::npos || lfSeparator < separator))
	{
		separator = lfSeparator;
		separatorSize = 2;
	}
	if (separator == std::string::npos || separator > 16384)
		return handleErrorResponse(serverConfig, BAD_GATEWAY);

	Response response;
	response.version = "HTTP/1.1";
	response.statusCode = 200;
	response.reasonPhrase = "OK";
	bool hasStatus = false;
	bool hasType = false;
	bool hasLocation = false;
	std::string headers;
	std::istringstream lines(output.substr(0, separator));
	std::string line;
	while (std::getline(lines, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		size_t colon = line.find(':');
		if (colon == std::string::npos || colon == 0)
			return handleErrorResponse(serverConfig, BAD_GATEWAY);
		std::string name = line.substr(0, colon);
		// Validate names and values before copying script-controlled headers into HTTP.
		for (size_t i = 0; i < name.size(); ++i)
		{
			if (name[i] >= 'A' && name[i] <= 'Z')
				name[i] = name[i] - 'A' + 'a';
			if (!(name[i] >= 'a' && name[i] <= 'z') && !(name[i] >= '0' && name[i] <= '9')
				&& std::string("!#$%&'*+-.^_`|~").find(name[i]) == std::string::npos)
				return handleErrorResponse(serverConfig, BAD_GATEWAY);
		}
		std::string value = line.substr(colon + 1);
		size_t first = value.find_first_not_of(" \t");
		value = first == std::string::npos ? "" : value.substr(first);
		size_t last = value.find_last_not_of(" \t");
		if (last != std::string::npos)
			value.erase(last + 1);
		for (size_t i = 0; i < value.size(); ++i)
		{
			unsigned char c = static_cast<unsigned char>(value[i]);
			if ((c < 32 && c != '\t') || c == 127)
				return handleErrorResponse(serverConfig, BAD_GATEWAY);
		}
		if (name == "status")
		{
			if (hasStatus || value.size() < 3 || value[0] < '2' || value[0] > '5'
				|| value[1] < '0' || value[1] > '9' || value[2] < '0' || value[2] > '9'
				|| (value.size() > 3 && value[3] != ' '))
				return handleErrorResponse(serverConfig, BAD_GATEWAY);
			response.statusCode = (value[0] - '0') * 100 + (value[1] - '0') * 10 + value[2] - '0';
			response.reasonPhrase = value.size() > 4 ? value.substr(4) : "CGI Response";
			hasStatus = true;
			continue;
		}
		if (name == "content-type")
		{
			if (hasType || value.empty())
				return handleErrorResponse(serverConfig, BAD_GATEWAY);
			hasType = true;
		}
		if (name == "location")
		{
			if (hasLocation || value.empty())
				return handleErrorResponse(serverConfig, BAD_GATEWAY);
			hasLocation = true;
		}
		// The server owns connection framing; do not trust a script's length or chunking.
		if (name == "content-length" || name == "transfer-encoding" || name == "connection"
			|| name == "keep-alive" || name == "upgrade" || name == "trailer" || name == "proxy-connection")
			continue;
		// Keep repeated headers (especially Set-Cookie) instead of overwriting them in a map.
		headers += name + ": " + value + "\r\n";
	}
	if (!hasStatus && hasLocation)
	{
		response.statusCode = 302;
		response.reasonPhrase = "Found";
	}
	if (!hasType && !hasLocation && response.statusCode != 204 && response.statusCode != 304)
		return handleErrorResponse(serverConfig, BAD_GATEWAY);
	response.body = output.substr(separator + separatorSize);
	if (response.statusCode == 204 || response.statusCode == 304)
		response.body.clear();
	else
		response.headers["Content-Length"] = std::to_string(response.body.size());
	response.headers["Connection"] = "close";
	_response = response;
	prepareResponseForSending();
	_writeBuffer.insert(_writeBuffer.find("\r\n\r\n") + 2, headers);
}
