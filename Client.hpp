#pragma once

# include "Request.hpp"
# include "RequestParser.hpp"

# include <string>

class Client
{
	private:
		int				_fd;
		std::string		_readBuffer;
		std::string		_writeBuffer;

		Request			_request; //current request
		RequestParser	_parser;
};