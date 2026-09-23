// #pragma once

// # include "Http.hpp"
// # include <string>
// # include <iostream>
// # include <map>
// # include <vector>
// # include <algorithm>
// # include "Config.hpp"

// class ResponseBuilder
// {
// 	public:
// 		Response buildResponse(const Request& request, const ServerConfig& serverConfig);
// 		Response buildGetResponse(const Request& request, const ServerConfig& serverConfig);
// 		Response buildErrorResponse(int statusCode, const ServerConfig& serverConfig);
// 		const LocationConfig* findLocation(const std::string& path, const ServerConfig& serverConfig);
// 		std::string serialize(const Response& response);
// 	private:
// 		bool getSource(const std::string& path, std::string& content);
// };

#pragma once

# include "Http.hpp"
# include "Config.hpp"
# include <string>
# include <iostream>
# include <map>
# include <string>
# include <vector>
# include <fstream>
# include <sstream>

class ResponseBuilder
{
	public:
		Response buildResponse(const Request& request,const ServerConfig& serverConfig);
		Response buildErrorResponse(int statusCode, const ServerConfig& serverConfig);
		std::string serialize(const Response& response);
	private:
		//based on  method response builders
		Response buildGetResponse(const Request& request, const ServerConfig& serverConfig, const LocationConfig& location);

		// Response buildPostResponse;
		// Response buildDeleteResponse;

		// Config / routing helpers
		const LocationConfig* findLocation(const std::string& path, const ServerConfig& serverConfig);
		bool isMethodAllowed(const std::string& method, const LocationConfig& location);

		// File/path helpers
		std::string resolveRoot(const ServerConfig& serverConfig, const LocationConfig& location);
		std::string resolveIndex(const ServerConfig& serverConfig,const LocationConfig& location);
		bool getSource(const std::string& path, std::string& content);
		std::string getReasonPhrase(int statusCode);
};
