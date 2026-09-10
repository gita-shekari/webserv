#include "RequestParser.hpp"
#include <iostream>
#include <cctype>
#include <limits>

RequestParser::RequestParser(): _section(REQUEST_LINE), _cursor(0), _contentLength(0), _body(NO_BODY), _chunkState(CHUNK_SIZE), _chunkSize(0){}

ParseStatus	RequestParser::parse(const std::string& buffer, Request& req)
{
	while (_section != DONE)
	{
		ParseStatus	status;

		if (_section == REQUEST_LINE)
			status = parseRequestLine(buffer, req);
		else if (_section == HEADERS)
			status = parseHeaders(buffer, req);
		else if (_section == BODY)
			status = parseBody(buffer, req);
		else
			return ERROR;
		if (status != COMPLETE)
			return status;
	}
	return COMPLETE;
}

size_t	RequestParser::getConsumedBytes() const
{
	return _cursor;
}

void	RequestParser::reset()
{
	_section = REQUEST_LINE;
	_cursor = 0;
	_contentLength = 0;
	_body = NO_BODY;
	_chunkState = CHUNK_SIZE;
	_chunkSize = 0;
}

std::string	sectionToString(ParseSection section)
{
	if (section == REQUEST_LINE)
		return "request line";
	else if (section == HEADERS)
		return "headers";
	else if (section == BODY)
		return "body";
	else if (section == DONE)
		return "done";
	else
		return "unknown";
}

std::string bodyToString(BodyType type)
{
	if (type == NO_BODY)
		return "no body";
	else if (type == CHUNKED_BODY)
		return "chunked body";
	else if (type == CONTENT_LENGTH_BODY)
		return "content length body";
	else
		return "unknown";
}

void	RequestParser::printAttributes() const
{
	std::cout << " - current section: " << sectionToString(_section) << std::endl;
	std::cout << " - body type: " << bodyToString(_body) << std::endl;
	std::cout << " - content length: " << _contentLength << std::endl;
	std::cout << " - last chunk size: " << _chunkSize << std::endl;
	std::cout << "--->" << std::endl;
}

ParseStatus	RequestParser::parseRequestLine(const std::string& buffer, Request& req)
{
	size_t	lineEnd = buffer.find("\r\n", _cursor);

	if (lineEnd == std::string::npos)
		return INCOMPLETE;

	std::string line = buffer.substr(_cursor, lineEnd - _cursor);

	size_t	firstSpace = line.find(' ');
	if (firstSpace == std::string::npos || firstSpace == 0)
		return ERROR;

	size_t	secondSpace = line.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos
		|| secondSpace == firstSpace + 1
		|| secondSpace + 1 >= line.size())
		return ERROR;

	req.method = line.substr(0, firstSpace);
	req.rawTarget = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	req.version = line.substr(secondSpace + 1);
	if (req.version != "HTTP/1.1")
		return ERROR;

	size_t	qMark = req.rawTarget.find('?');
	if (qMark == std::string::npos)
		req.path = req.rawTarget;
	else
	{
		req.path = req.rawTarget.substr(0, qMark);
		req.query = req.rawTarget.substr(qMark + 1);
	}
	_cursor = lineEnd + 2;
	_section = HEADERS;
	return COMPLETE;
}

ParseStatus	RequestParser::parseHeaders(const std::string& buffer, Request& req)
{
	size_t	headerEnd = buffer.find("\r\n\r\n", _cursor);
	if (headerEnd == std::string::npos)
		return INCOMPLETE;

	while (_cursor < headerEnd)
	{
		size_t	lineEnd = buffer.find("\r\n", _cursor);
		if (lineEnd == std::string::npos || lineEnd > headerEnd)
			return ERROR;

		std::string line =  buffer.substr(_cursor, lineEnd - _cursor);

		size_t colon = line.find(':');
		if (colon == std::string::npos || colon == 0)
			return ERROR;

		std::string	key = line.substr(0, colon);
		if (key[0] == ' ' || key[0] == '\t' || key[key.size() - 1] == ' ' || key[key.size() - 1] == '\t')
    		return ERROR;
		key = toLower(key);

		std::string	value = trim(line.substr(colon + 1));

		std::map<std::string, std::string>::iterator it;
		it = req.headers.find(key);
		if (it == req.headers.end())
			req.headers[key] = value;
		else
		{
			if (key == "content-length" || key == "transfer-encoding" || key == "host") // Reject duplicates for fields that require special handling
				return ERROR;
			it->second += ", " + value;
		}
		_cursor = lineEnd + 2;
	}
	std::map<std::string, std::string>::const_iterator host;
	host = req.headers.find("host");
	if (host == req.headers.end() || host->second.empty())
		return ERROR;

	_cursor = headerEnd + 4;
	return (judgeBody(req));
}

ParseStatus	RequestParser::parseBody(const std::string& buffer, Request& req)
{
	if (_body == CONTENT_LENGTH_BODY)
		return parseContentLengthBody(buffer, req);
	if (_body == CHUNKED_BODY)
		return parseChunkedBody(buffer, req);
	return ERROR;
}

ParseStatus	RequestParser::judgeBody(const Request& req)
{
	std::map<std::string, std::string>::const_iterator itTE;
	std::map<std::string, std::string>::const_iterator itCL;
	itTE = req.headers.find("transfer-encoding");
	itCL = req.headers.find("content-length");

	if (itTE != req.headers.end())
	{
		if (toLower(itTE->second) != "chunked" || itCL != req.headers.end())
			return ERROR;
		_section = BODY;
		_body = CHUNKED_BODY;
		return COMPLETE;
	}
	else if (itCL != req.headers.end())
	{
		if (!parseDecSize(itCL->second, _contentLength))
			return ERROR;
		_section = BODY;
		_body = CONTENT_LENGTH_BODY;
		return COMPLETE;
	}
	_section = DONE;
	_body = NO_BODY;
	return COMPLETE;
}

ParseStatus	RequestParser::parseContentLengthBody(const std::string& buffer, Request& req)
{
	size_t	bodyLength = buffer.size() - _cursor;
	if (bodyLength < _contentLength)
		return INCOMPLETE;
	req.body = buffer.substr(_cursor, _contentLength);
	_cursor += _contentLength;
	_section = DONE;
	return COMPLETE;
}

ParseStatus	RequestParser::parseChunkedBody(const std::string& buffer, Request& req)
{
	while (true)
	{
		if (_chunkState == CHUNK_SIZE)
		{
			size_t	sizeEnd = buffer.find("\r\n", _cursor);
			if (sizeEnd == std::string::npos)
				return INCOMPLETE;
			std::string	sizeLine = buffer.substr(_cursor, sizeEnd - _cursor);

			size_t	semicolon = sizeLine.find(';');
			std::string	sizeNum;
			if (semicolon == std::string::npos)
				sizeNum = sizeLine;
			else
				sizeNum = sizeLine.substr(0, semicolon);

			if (!parseHexSize(sizeNum, _chunkSize))
				return ERROR;
			_cursor = sizeEnd + 2;
			if (_chunkSize == 0)
			{
				if (buffer.size() - _cursor < 2)
					return INCOMPLETE;
				if (buffer[_cursor] != '\r' || buffer[_cursor + 1] != '\n')
					return ERROR;
				_cursor += 2;
				_section = DONE;
				return COMPLETE;
			}
			_chunkState = CHUNK_DATA;
		}
		if (_chunkState == CHUNK_DATA)
		{
			size_t	bodyLength = buffer.size() - _cursor;
			if (bodyLength < _chunkSize || bodyLength - _chunkSize < 2)
				return INCOMPLETE;
			if (buffer[_cursor + _chunkSize] != '\r' || buffer[_cursor + _chunkSize + 1] != '\n')
				return ERROR;
			req.body.append(buffer, _cursor, _chunkSize);
			_cursor += _chunkSize + 2;
			_chunkState = CHUNK_SIZE;
		}
	}
}

bool	RequestParser::parseDecSize(const std::string& str, size_t& size)
{
	if (str.empty())
		return false;
	size_t	value = 0;
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] < '0' || str[i] > '9')
			return false;
		size_t	digit = str[i] - '0';
		if (value > (std::numeric_limits<size_t>::max() - digit) / 10)
   			 return false;
		value = value * 10 + digit;
	}
	size = value;
	return true;
}

bool	RequestParser::parseHexSize(const std::string& str, size_t& size)
{
	if (str.empty())
		return false;

	size_t	ret = 0;
	for (size_t	i = 0; i < str.size(); ++i)
	{
		char c = str[i];
		size_t	v = 0;
		if (c >= '0' && c <= '9')
			v = c - '0';
		else if (c >= 'a' && c <= 'f')
			v = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			v = c - 'A' + 10;
		else
			return false;
		if (ret > (std::numeric_limits<size_t>::max() - v) / 16)
			return false;
		ret = ret * 16 + v;
	}
	size = ret;
	return true;
}

std::string	RequestParser::toLower(const std::string& str)
{
	std::string	ret = str;
	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = std::tolower(static_cast<unsigned char>(ret[i]));
	return ret;
}

std::string	RequestParser::trim(const std::string& str)
{
	size_t	start = 0;
	size_t	end = str.size();

	while (start < end && (str[start] == ' ' || str[start] == '\t'))
		++start;
	while (end >start && (str[end - 1] == ' ' || str[end - 1] == '\t'))
		--end;
	return str.substr(start, end - start);
}
