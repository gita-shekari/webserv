#include "ConfigParser.hpp"

#include <fstream>
#include <cctype>

ConfigParser::ConfigParser(){}

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
	// current token is "location"
	_current++;
	if (_current >= _tokens.size())
		throw std::runtime_error("Missing path after location");
	lc.path = _tokens[_current];
	_current++;
	if (_current >= _tokens.size() || _tokens[_current] != "{")
		throw std::runtime_error("Expected '{' after location");
	_current++;
	while (_current < _tokens.size() && _tokens[_current] != "}")
	{
		if (_tokens[_current] == "methods")
		{
			_current++;
			if (_current >= _tokens.size())
				throw std::runtime_error("Expected method name");
			while (_current < _tokens.size() && _tokens[_current] != ";")
			{
				lc.methods.push_back(_tokens[_current]);
				_current++;
			}
			if (_current >= _tokens.size())
				throw std::runtime_error("Expected ';' after methods");
			_current++;
		}
		else if (_tokens[_current] == "root")
		{
			_current++;
			if (_current >= _tokens.size())
				throw std::runtime_error("Expected root path");
			lc.root = _tokens[_current];
			_current++;
			if (_current >= _tokens.size() || _tokens[_current] != ";")
				throw std::runtime_error("Expected ';' after root");
			_current++;
		}
		else if (_tokens[_current] == "index")
		{
			_current++;
			if (_current >= _tokens.size())
				throw std::runtime_error("Expected index file");
			lc.index = _tokens[_current];
			_current++;
			if (_current >= _tokens.size() || _tokens[_current] != ";")
				throw std::runtime_error("Expected ';' after index");
			_current++;
		}
		else
			throw std::runtime_error("Unknown location directive: " + _tokens[_current]);
	}
	if (_current >= _tokens.size())
		throw std::runtime_error("Missing '}' for location block");
	_current++;
	return lc;
}

bool	parseMaxSize(size_t& size, std::string& input)
{
	if (input == "0")
	{
		size = ABS_MAX_CAP_BODY_SIZE;
		return (true);
	}
	size_t	i = 0;
	unsigned long long parse_val = 0;
	try
	{
		parse_val = std::stoull(input, &i);
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error parsing client_max_size: " << e.what() << '\n';
		return false;
	}
	size_t	multiplier = 1;
	if (i < input.size())
	{
		char unit = std::tolower(input[i]);
		if (input.size() - i > 1)
		{
			std::cerr << "Config Error: invaild trailing charaters in size units " << input[i] << std::endl;
			return false;
		}
		if (unit == 'k')
			multiplier = 1024;
		else if (unit == 'm')
			multiplier = 1024 * 1024;
		else if (unit == 'g')
			multiplier = 1024 * 1024 * 1024;
		else 
		{
			std::cerr << "Config Error: unknown unit suffix" << std::endl;
			return false;
		}
	}

	if (parse_val > std::numeric_limits<size_t>::max() / multiplier)
	{
		std::cerr << "Config Error: client_max_body_size number overflow" << std::endl;
		return false;
	}
	size_t res = parse_val * multiplier;
	if (res > ABS_MAX_CAP_BODY_SIZE)
	{
		std::cerr << "Config Error: client_max_body_size exceeds the server's memory buffer cap of 100 M. " << std::endl;
		return false;
	}
	size = res;
	return true;
}

ServerConfig ConfigParser::parseServer()
{
	ServerConfig sc;
	// current token is "server"
	//opening {
	_current++;
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
		else if (_tokens[_current] == "client_max_body_size")
		{
			_current++;
			if (_current >= _tokens.size())
				throw std::runtime_error("Missing Max body size");
			if (!parseMaxSize(sc.client_max_body_size, _tokens[_current]))
				throw std::runtime_error("");
			_current++;
			if (_current >= _tokens.size() || _tokens[_current] != ";")
				throw std::runtime_error("Expected ';' after root");
			_current++;
		}
		else if (_tokens[_current] == "error_page")
		{
			_current++;
			if (_current >= _tokens.size())
				throw std::runtime_error("Missing error_page");
			sc.error_page = _tokens[_current];
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
		<< ", max="
		<< sc.client_max_body_size
		<< ", error_page="
		<< sc.error_page;
		std::cout << ", locations=";
		for(size_t i = 0; i < sc.locations.size(); i++)
			std::cout << sc.locations[i].path;
		std::cout << std::endl;
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
