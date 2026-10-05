#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>

// called RequestErr before. change when we merge the code.
enum	HttpStatus
{
	REQ_OK = 200,
	MOVED_PERMANENTLY = 301,
	BAD_REQ = 400,
	FORBIDDEN = 403,
	PAGE_NOT_FOUND = 404,
	METHODE_NOT_ALLOWED = 405,
	PLAYLOAD_TOO_LARGE = 413,
	INTERNAL_SERVER_ERR = 500,
	NOT_IMPLEMENTED = 501,
	HTTP_VERSION_NOT_NSUP = 505
};

struct Request
{
	std::string	method;
	std::string rawTarget;
	std::string	path;
	std::string	query;
	std::string	version;
	HttpStatus	httpStatus = REQ_OK;

	size_t		effectiveMaxBodySize = static_cast<size_t>(-1);

	std::map<std::string, std::string> headers;

	std::string	body;

	void	clear();

	//debug
	void	printRequest();
};


struct Response
{
	std::string							version;
	int									statusCode;
	std::string							reasonPhrase;
	std::map<std::string, std::string>	headers;
	std::string							body;
};
