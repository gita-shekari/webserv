#pragma once

#include "Config.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <cctype>

class ConfigParser
{
	private:
		std::vector<std::string>	_tokens;
		size_t						_current;
		void						tokenize(std::ifstream& file);
		ServerConfig				parseServer();
		LocationConfig				parseLocation();
		const std::string&			currentToken() const;
		bool						isValidPort(const std::string& token);

		void						validateUniquePorts(const std::vector<ServerConfig>& configs, int newPort);

	public:
		ConfigParser();
		std::vector<ServerConfig>	parseConfig(const std::string& filename);
};
