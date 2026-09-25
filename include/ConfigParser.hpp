#pragma once

#include "Config.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#pragma once

#include "Config.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <cctype>
class ConfigParser
{
	private:
		std::vector<std::string>	_tokens;
		size_t						_current;

		void						tokenize(std::ifstream& file);

		ServerConfig				parseServer();
		LocationConfig				parseLocation();

		void						parseHost(ServerConfig& sc);
		void						parseListen(ServerConfig& sc);
		void						parseRoot(std::string& root);
		void						parseClientMaxBodySize(ServerConfig& sc);
		void						parseErrorPage(ServerConfig& sc);

		void 						validateServer(const ServerConfig& sc);
		void						expect(const std::string& expected);
		const std::string&			currentToken() const;
		bool						isValidPort(const std::string& token);
		size_t 						extract_size(const std::string& token);
	public:
		ConfigParser();
		std::vector<ServerConfig>	parseConfig(const std::string& filename);
};
