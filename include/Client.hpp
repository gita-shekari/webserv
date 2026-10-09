#pragma once

# include "Http.hpp"
# include "RequestParser.hpp"
# include "ResponseBuilder.hpp"

# include <string>
# include <iostream>
# include <cstddef> //for size_t
# include <chrono> // for steady_clock

//enum	ConfigBehavior
//{
//	REDIRECT,
//	CGI,
//	UPLOAD,
//	DELETE,
//	DIRECTORU_LISTING,
//	STATIC,
//	METHOD_NOT_FOUND
//};


class Client
{
	public:
		Client() = default;
		Client(int fd, size_t serverIndex);
		~Client(void);

		//getters
		int			getFd(void);
		size_t		getServerIndex(void) const;
		size_t		getLocationIndex(void) const;
		size_t		getEffectiveMaxBodySize(void) const;
		bool		getIsConnected(void);
		std::string getWriteBuffer();
		Request		getReq(void);
		std::chrono::steady_clock::time_point	getLastActivity(void) const;

		// setters
		void		setRequestPath(const std::string& str); // for testing
		void		setEffectiveMaxBodySize(size_t size);
		void		disConnected(void);

		// client util
		bool		isURIAllowed(struct stat* buf, std::string path);
		
		// for request processing
		void		appendReadBuffer(const char *data, size_t size);
		ParseStatus	parseReadBuffer(void);
		void		matchLocation(const std::vector<LocationConfig>& location);

		// handle routings
		void 		routing(const ServerConfig& serverConfig);
		void		routingGet(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath);
		void		routingPost(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath);
		void		routingDelete(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath);

		void		handleErrorResponse(const ServerConfig& sc, HttpStatus code);
		void 		handleDirectoryListing(const ServerConfig& serverConfig, std::string& fullPath);
		// handleRedirect()
		// handleStatic()
		// handleCGI()

		// for timer
		void		updateLastActivity(void);

	private:
		int				_fd = -1;
		size_t			_serverIndex;
		size_t			_locationIndex = static_cast<size_t>(-1);
		bool			_isConnected = false;
		std::string		_readBuffer;
		std::string		_writeBuffer;

		Request			_request; //current request
		RequestParser	_parser;
		Response		_response;
		ResponseBuilder	_builder;

		std::chrono::steady_clock::time_point	_lastActivity;
};
