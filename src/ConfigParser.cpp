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
		for(size_t i = 0; i < line.size(); i++)
		{
			if(std::isspace(static_cast<unsigned char>(line[i])))
			{
				if (!token.empty())
				{
					_tokens.push_back(token);
					token.clear();
				}
			}
			else if(line[i] == '{' || line[i] == '}' || line[i] == ';')
			{
				if (!token.empty())
				{
					_tokens.push_back(token);
					token.clear();
				}
				_tokens.push_back(std::string(1, line[i]));
			}
			else
			{
				token+=line[i];
			}
		}
		if (!token.empty())
		{
			_tokens.push_back(token);
		}
	}
	for(size_t i = 0; i < _tokens.size(); i++)
		{
			std::cout << _tokens[i] << "\n";
		}
}
ServerConfig ConfigParser::parseServer()
{
	_current++;
	if(_tokens[_current] != "{")
	{
		throw std::runtime_error("the format of config file is not correct!");
	}
	while (_tokens[_current] != "}")
	{
		if(_tokens[_current] == "listen")
			
	}



}
std::vector<ServerConfig>	ConfigParser::parseConfig(const std::string& filename)
{
	std::ifstream conf(filename.c_str());
	if (!conf.is_open())
		throw std::runtime_error("Config file can not be opened");
	_current = 0;
	_tokens.clear();
	tokenize(conf);
	std::vector<ServerConfig> configs;
	while(_current < _tokens.size())
	{
		if (_tokens[_current] != "server")
			throw std::runtime_error("Expected server block");
		configs.push_back(parseServer());
	}
	return configs;
}
