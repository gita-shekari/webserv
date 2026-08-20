#pragma once

# include "Request.hpp"
//# include "RequestParser.hpp"

# include <string>
# include <iostream>

class Client
{
	public:
		Client(int fd);
		~Client(void);

		bool	getIsConnected(void);
		void	disConnected(void);

		int		getFd(void);
		void	setFd(void);

		// for appending information in buffer
		void	appendReadBuffer(char *buffer);

	private:
		int				_fd;
		bool			_isConnected;
		std::string		_readBuffer;
		std::string		_writeBuffer;

		Request			_request; //current request
		//RequestParser	_parser;
};