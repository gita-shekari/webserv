#pragma once

# include "Http.hpp"
# include "RequestParser.hpp"
# include "ResponseBuilder.hpp"

# include <string>
# include <iostream>
# include <cstddef> //for size_t
# include <chrono> // for steady_clock

class Client
{
	public:
		Client() = default;
		Client(int fd, size_t configIndex);
		~Client(void);

		//getters
		int			getFd(void);
		size_t		getConfigIndex(void) const;
		bool		getIsConnected(void);
		std::string getWriteBuffer();
		Request		getReq(void);
		std::chrono::steady_clock::time_point	getLastActivity(void) const;

		// setters
		void		disConnected(void);

		// for appending information in buffer
		ParseStatus	parseRequest(char *buffer);
		void 		prepareResponse(const ServerConfig& serverConfig);

		// for timer
		void		updateLastActivity(void);

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

		std::chrono::steady_clock::time_point	_lastActivity;
};