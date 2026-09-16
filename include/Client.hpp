#pragma once

# include "Http.hpp"
# include "RequestParser.hpp"
# include "ResponseBuilder.hpp"

# include <string>
# include <iostream>
# include <cstddef> //for size_t

class Client
{
	public:
		Client() = default;
		Client(int fd, size_t configIndex);
		~Client(void);

		bool		getIsConnected(void);
		void		disConnected(void);
		Request		getReq(void);
		std::string getWriteBuffer();
		int			getFd(void);

		size_t	getConfigIndex(void) const;

		// for appending information in buffer
		ParseStatus		parseRequest(char *buffer);
		void 			prepareResponse(const ServerConfig& serverConfig);

	private:
		int				_fd = -1;
		size_t			_configIndex = 0;
		bool			_isConnected = false;
		std::string		_readBuffer;
		std::string		_writeBuffer;

		Request			_request; //current request
		RequestParser	_parser;
		Response		_response;
		ResponseBuilder	_builder;
};