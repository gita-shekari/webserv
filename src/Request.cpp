#include "Request.hpp"
#include <iostream>

void Request::clear()
{
	method.clear();
	rawTarget.clear();
	path.clear();
	query.clear();
	version.clear();
	headers.clear();
	body.clear();
}

void	Request::printRequest()
{
	std::cout << "method = " << "\"" << method << "\"" << std::endl;
	std::cout << "rawTarget = " << "\"" << rawTarget << "\"" << std::endl;
	std::cout << "path = " << "\"" << path << "\"" << std::endl;
	std::cout << "query = " << "\"" << query << "\"" << std::endl;
	std::cout << "version = " << "\"" << version << "\"" << std::endl;
	
	std::cout << "\nheaders:" << std::endl;

	std::map<std::string, std::string>::const_iterator it;
	for (it = headers.begin(); it != headers.end(); ++it)
		std::cout << it->first << ": " << it->second << std::endl;
	
	std::cout << "\nbody = \"" << body << "\"" << std::endl;
}