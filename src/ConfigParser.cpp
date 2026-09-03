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
	// for(size_t i = 0; i < _tokens.size(); i++)
	// 	{
	// 		std::cout << _tokens[i] << "\n";
	// 	}
}
bool ConfigParser::isValidPort(const std::string& token)
{
	if(token.empty())
		return false;
	for(size_t i = 0; i < token.size(); i++)
	{
		if(!std::isdigit(static_cast<unsigned char>(token[i])))
			return false;
	}
	long port = strtol(token.c_str(), NULL, 10);
	if (port < 1 || port > 65535)
		return false;
	return true;
}
LocationConfig ConfigParser::parseLocation()
{
	LocationConfig lc;
	_current++;
	return lc;
}
ServerConfig ConfigParser::parseServer()
{
	ServerConfig sc;
	// current token is "server"
	//opening {
	_current++;
	std::cout << _tokens[_current] << '\n';
	if (_current >= _tokens.size() || _tokens[_current] != "{")
		throw std::runtime_error("Expected '{' after server");
	_current++;
	while (_current < _tokens.size() && _tokens[_current] != "}")
	{
		if (_tokens[_current] == "listen")
		{
			_current++;
			if (_current >= _tokens.size())
				throw std::runtime_error("Missing port after listen");
			if (!isValidPort(_tokens[_current]))
				throw std::runtime_error("Invalid port");
			sc.port = std::atoi(_tokens[_current].c_str());
			_current++;
			if (_current >= _tokens.size() || _tokens[_current] != ";")
				throw std::runtime_error("Expected ';' after listen");
			_current++;
		}
		else if (_tokens[_current] == "root")
		{
			_current++;
			if (_current >= _tokens.size())
				throw std::runtime_error("Missing root path");
			sc.root = _tokens[_current];
			_current++;
			if (_current >= _tokens.size() || _tokens[_current] != ";")
				throw std::runtime_error("Expected ';' after root");
			_current++;
		}
		else if (_tokens[_current] == "location")
			sc.locations.push_back(parseLocation());
		else
			throw std::runtime_error("Unknown server config: " + _tokens[_current]);
	}
	if (_current >= _tokens.size())
		throw std::runtime_error("Missing '}' for server block");
	_current++;
	std::cout
		<< "port="
		<< sc.port
		<< ", root="
		<< sc.root
		<< ", locations="
		<< sc.locations.size()
		<< std::endl;
	return sc;
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
