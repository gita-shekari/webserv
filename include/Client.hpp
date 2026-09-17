#pragma once

# include "Http.hpp"
# include "RequestParser.hpp"
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

		// for appending information in buffer
		ParseStatus		parseRequest(char *buffer, const ServerConfig& ServerConfig);
		void 			prepareResponse(const ServerConfig& serverConfig);

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
