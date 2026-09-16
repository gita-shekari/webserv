#pragma once

# include <string>
# include <map>

// called RequestErr before. change when we merge the code.
enum	HttpStatus
{
	REQ_OK = 200,
	BAD_REQ = 400,
	PAGE_NOT_FOUND = 404,
	METHONDE_NOT_ALLOWED = 405,
	PLAYLOAD_TOO_LARGE = 413,
	NOT_IMPLEMENTED = 501,
	HTTP_VERSION_NOT_NSUP = 505
};

struct Request
{
	std::string	method;
	std::string rawTarget;	// for debugging
	std::string	path;
	std::string	query;
	std::string	version;
	HttpStatus	httpStatus = REQ_OK;

	std::map<std::string, std::string> headers;

	std::string	body;

	void	clear();

	//debug
	void	printRequest();
};
