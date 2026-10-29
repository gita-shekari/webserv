#pragma once

#include "Config.hpp"
#include <string>

namespace RoutingUtils
{
	bool isMethodAllowed(const LocationConfig& lc, const std::string& method);

	std::string joinPath(const std::string& root, const std::string& path);

	std::string getCorrectFullPath(const ServerConfig& sc, const LocationConfig& lc, std::string reqPath);

	bool isCGI(const LocationConfig& lc, const std::string& path);
}