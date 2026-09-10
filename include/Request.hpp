#pragma once

# include <string>
# include <map>

struct Request
{
	std::string	method;
	std::string rawTarget;	// for debugging
	std::string	path;
	std::string	query;
	std::string	version;

	std::map<std::string, std::string> headers;

	std::string	body;

	void	clear();

	//debug
	void	printRequest();
};
