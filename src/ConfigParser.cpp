#include "ConfigParser.hpp"

ConfigParser::ConfigParser()
{

}
void ConfigParser::tokenize(std::ifstream& file)
{
	std::string line;
	while(std::getline(file, line))
	{
		std::string token;
		
		std::cout << line << "\n";
	}

}
std::vector<ServerConfig>	ConfigParser::parseConfig(const std::string& filename)
{
	std::ifstream conf(filename.c_str());
	if (!conf.is_open())
		throw std::runtime_error("Config file can not be opened");
	tokenize(conf);
	// parsing comes later
	std::vector<ServerConfig> configs;
	return configs;

}
