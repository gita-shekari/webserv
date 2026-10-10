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
		Response buildErrorResponse(int statusCode, const ServerConfig& serverConfig);
		Response buildStaticResponse(const std::string& filePath, const ServerConfig& serverConfig);
		Response buildRedirectResponse(int statusCode, const std::string& location);
		Response buildNoContentResponse();
		Response buildCreatedResponse(const std::string& location);
		Response buildListingResponse(const std::vector<std::string>& list, const std::string& requestPath);
		std::string serialize(const Response& response);
	private:
		

		// File/path helpers
		std::string getRoot(const ServerConfig& serverConfig, const LocationConfig& location);
		std::string getIndex(const LocationConfig& location);
		bool getSource(const std::string& path, std::string& content);
		std::string getReasonPhrase(int statusCode);
		std::string getContentType(const std::string& path);
};
