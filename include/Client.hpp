#pragma once

# include "Request.hpp"
# include "RequestParser.hpp"
#include  "Response.hpp"
#include  "ResponseBuilder.hpp"
# include <string>
# include <iostream>

class Client
{
	public:
		Client() = default;
		Client(int fd);
		~Client(void);

		bool		getIsConnected(void);
		void		disConnected(void);
		Request		getReq(void);
		std::string getWriteBuffer();
		int			getFd(void);
		void		setFd(void); // no need?

		// for appending information in buffer
		ParseStatus		parseRequest(char *buffer);
		void buildResponse(const ServerConfig& serverConfig);

	private:
		int				_fd = -1;
		bool			_isConnected = false;
		std::string		_readBuffer;
		std::string		_writeBuffer;

		Request			_request; //current request
		RequestParser	_parser;
		Response		_response;
		ResponseBuilder	_builder;

};
