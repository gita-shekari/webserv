#pragma once

# include <string>
# include <map>

enum	RequestErr
{
	REQ_OK = 0,
	BAD_REQ = 400,
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
	RequestErr	errtype = REQ_OK;

	std::map<std::string, std::string> headers;

	std::string	body;

	void	clear();

	//debug
	void	printRequest();
};