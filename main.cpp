#include "RequestParser.hpp"
#include "Request.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

std::string	statusToString(ParseStatus status)
{
	if (status == INCOMPLETE)
		return "Incomplete";
	else if (status == COMPLETE)
		return "Complete";
	else if (status == ERROR)
		return "Error";
	else
		return "Unknown";
}

int main(int argc, char* argv[])
{
	if (argc != 2)
		std::cerr << "Error" << std::endl;

	std::ifstream file(argv[1]);
	if (!file)
		return 1;
	
	std::stringstream	ss;
	ss << file.rdbuf();

	std::string buffer = ss.str();

	Request	req;
	RequestParser	parser;

	ParseStatus status = parser.parse(buffer, req);
	std::cout << "------------------\nParsing Status: " << statusToString(status) << std::endl;
	parser.printAttributes();
	req.printRequest();
	// req.clear();

	// code for partial_chunk test
	buffer += "ld\r\n0\r\n\r\n";
	status = parser.parse(buffer, req);
	std::cout << "------------------\nParsing Status: " << statusToString(status) << std::endl;
	parser.printAttributes();
	req.printRequest();

	return 0;
}