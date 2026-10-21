#pragma once

# include "Http.hpp"
# include "RequestParser.hpp"
# include "ResponseBuilder.hpp"
#include <sys/stat.h>
#include <dirent.h>
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

enum ClientState
{
	READ_REQ,
	REQ_READY,
	CGI_RUNNING,
	RESPONSE_READY,
	SENDING_RESPONSE
	//CLOSE_CLIENT
};

class Client
{
	public:
		Client() = default;
		Client(int fd, size_t serverIndex);
		~Client(void);

		//getters
		int			getFd(void) const;
		size_t		getServerIndex(void) const;
		size_t		getLocationIndex(void) const;
		size_t		getEffectiveMaxBodySize(void) const;
		bool		getIsConnected(void) const;
		const std::string& getWriteBuffer() const;
		// no one is using getReq()?
		Request		getReq(void) const;
		std::chrono::steady_clock::time_point	getLastActivity(void) const;
		ssize_t		getWriteOffSet() const;
		bool 		getResponseisReady() const;
		ClientState getState() const;


		// setters
		void		setEffectiveMaxBodySize(size_t size);
		void		disConnected(void);
		void		setWriteOffSet(ssize_t byte);
		void		setState(ClientState st);

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
		void		routingDelete(const ServerConfig& sc, std::string& fullPath);

		void		handleErrorResponse(const ServerConfig& sc, HttpStatus code);
		bool		isDirectoryEntry(const std::string& fullPath, const std::string& name);
		bool		readDirectoryEntries(DIR* dir, const std::string& fullPath, std::vector<std::string>& entries);
		void		handleDirectoryOpenError(const ServerConfig& serverConfig);
		void		handleDirectoryListing(const ServerConfig& serverConfig, const std::string& fullPath);
		void 		handleCGI(const ServerConfig& serverConfig, const std::string& scriptPath);
		void		handleStatic(const ServerConfig& serverConfig, const std::string& filePath);
		void		handleRedirect(int code, const std::string& redirURI);
		void		handleDelete(const ServerConfig& serverConfig, const std::string& filePath);
		void		handleUpload(const ServerConfig& serverConfig, const LocationConfig& lc);
		void		prepareResponseForSending();
		void		updateLastActivity(void);

	private:
		int				_fd = -1;
		size_t			_serverIndex;
		size_t			_locationIndex = static_cast<size_t>(-1);
		bool			_isConnected = false;
		std::string		_readBuffer;
		std::string		_writeBuffer;
		ssize_t			_writeOffSet;

		Request			_request; //current request
		RequestParser	_parser;
		Response		_response;
		ResponseBuilder	_builder;
		ClientState		_state;
		std::chrono::steady_clock::time_point	_lastActivity;
};
