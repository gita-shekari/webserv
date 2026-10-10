#pragma once

#include "Config.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include <limits>

class ConfigParser
{
private:
	std::vector<std::string> _tokens;
	size_t _current;

	void tokenize(std::ifstream& file);
	void expect(const std::string& expected);

	bool isValidPort(const std::string& token);
	bool isValidHost(const std::string& token);
	bool isValidMethod(const std::string& method);

	size_t extract_size(const std::string& token);

	ServerConfig parseServer();
	LocationConfig parseLocation();

	void parseListen(ServerConfig& sc);
	void parseHost(ServerConfig& sc);
	void parseRoot(std::string& root);
	void parseClientMaxBodySize(ServerConfig& sc);
	void parseErrorPage(ServerConfig& sc);

	void parseMethods(LocationConfig& lc);
	void parseIndex(LocationConfig& lc);
	void parseAutoindex(LocationConfig& lc);
	void parseClientMaxBodySize(LocationConfig& lc);
	void parseUpload(LocationConfig& lc);
	void parseUploadStore(LocationConfig& lc);
	void parseRedirect(LocationConfig& lc);
	void parseCgiExtension(LocationConfig& lc);
	void parseCgiPath(LocationConfig& lc);

	void validateServer(const ServerConfig& sc);
	void validateLocation(const LocationConfig& lc);

public:
	ConfigParser();

	std::vector<ServerConfig>
	parseConfig(const std::string& filename);
};
