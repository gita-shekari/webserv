#pragma once

#include "Request.hpp"
#include "Response.hpp"
#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include "Config.hpp"

class ResponseBuilder
{
	public:
		Response build(const Request& request, const ServerConfig& serverConfig);
		Response buildGetResponse(const Request& request, const ServerConfig& serverConfig);
		const LocationConfig* findLocation(const std::string& path, const ServerConfig& serverConfig);
		std::string serialize(const Response& response);
	private:
		bool getSource(const std::string& path, std::string& content);
};
