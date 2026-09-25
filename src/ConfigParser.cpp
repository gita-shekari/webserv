#include "ConfigParser.hpp"

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
bool ConfigParser::isValidHost(const std::string& token)
{
	if (token.empty())
		return false;
	size_t i = 0;
	for (size_t section = 0; section < 4; section++)
	{
		if (i >= token.size())
			return false;
		int value = 0;
		size_t digits = 0;
		while (i < token.size() && token[i] != '.')
		{
			if (!std::isdigit(static_cast<unsigned char>(token[i])))
				return false;
			value = value * 10 + (token[i] - '0');
			digits++;
			if (digits > 3 || value > 255)
				return false;
			i++;
		}
		if (digits == 0)
			return false;
		if (section < 3)
		{
			if (i >= token.size() || token[i] != '.')
				return false;
			i++;
		}
		else if (i != token.size())
			return false;
	}
	return true;
}
size_t ConfigParser::extract_size(const std::string& token)
{
	if(token.empty())
		throw std::runtime_error("Invalid client_max_body_size");
	size_t multiplier = 1;
	char suffix = token[token.size() - 1]; // client_max_body_size 10m -> suffix is m
	std::string number = token;
	if (suffix == 'k' || suffix == 'K')
	{
		multiplier = 1024;
		number = token.substr(0, token.size() - 1);
	}
	else if (suffix == 'm' || suffix == 'M')
	{
		multiplier = 1024 * 1024;
		number = token.substr(0, token.size() - 1);
	}
	if (number.empty())
		throw std::runtime_error("Invalid client_max_body_size");
	for (size_t i = 0; i < number.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(number[i])))
			throw std::runtime_error("Invalid client_max_body_size");
	}
	size_t size = static_cast<size_t>(std::stoull(number));
	return size * multiplier;
}
void ConfigParser::parseClientMaxBodySize(ServerConfig& sc)
{
	
}
LocationConfig ConfigParser::parseLocation()
{
	LocationConfig lc;
	expect("location");

	if (_current >= _tokens.size() || _tokens[_current] == "{")
		throw std::runtime_error("Missing location path");
	lc.path = _tokens[_current++];
	expect("{");
	while (_current < _tokens.size() && _tokens[_current] != "}")
	{
		if (_tokens[_current] == "methods")
			parseMethods(lc);
		else if (_tokens[_current] == "root")
			parseRoot(lc.root);
		else if (_tokens[_current] == "index")
			parseIndex(lc);
		else if (_tokens[_current] == "autoindex")
			parseAutoindex(lc);
		else if (_tokens[_current] == "client_max_body_size")
			parseClientMaxBodySize(lc);
		else if (_tokens[_current] == "upload")
			parseUpload(lc);
		else if (_tokens[_current] == "upload_store")
			parseUploadStore(lc);
		else if (_tokens[_current] == "return")
			parseRedirect(lc);
		else if (_tokens[_current] == "cgi_extension")
			parseCgiExtension(lc);
		else if (_tokens[_current] == "cgi_path")
			parseCgiPath(lc);
		else
			throw std::runtime_error(
				"Unknown location directive: " + _tokens[_current]);
	}
	expect("}");
	validateLocation(lc);
	return lc;
}
void ConfigParser::expect(const std::string& expected)
{
	 if (_current >= _tokens.size())
		throw std::runtime_error("Unexpected end of config");
	if (_tokens[_current] != expected)
		throw std::runtime_error("Unexpected token");
	_current++;
}
void ConfigParser::parseListen(ServerConfig& sc)
{
	expect("listen");
	if (_current >= _tokens.size())
		throw std::runtime_error("Missing listen value");
	if (!isValidPort(_tokens[_current]))
		throw std::runtime_error("Invalid port: " + _tokens[_current]);
	sc.port = std::atoi(_tokens[_current].c_str());
	_current++;
	expect(";");
}
void ConfigParser::parseHost(ServerConfig& sc)
{
	expect("host");
	if (_current >= _tokens.size())
		throw std::runtime_error("Missing host value");
	if (!isValidHost(_tokens[_current]))
		throw std::runtime_error("Invalid host: " + _tokens[_current]);
	sc.host = _tokens[_current];
	_current++;
	expect(";");
}
void ConfigParser::parseRoot(std::string& root)
{
	expect("root");
	if (_current >= _tokens.size() ||
		_tokens[_current] == ";" ||
		_tokens[_current] == "}")
		throw std::runtime_error("Missing root path");
	root = _tokens[_current];
	_current++;
	expect(";");
}
void ConfigParser::parseRoot(std::string& root)
{
	expect("root");
	if (!isValidHost(_tokens[_current]))
		throw std::runtime_error("Invalid host");
	sc.port = std::atoi(_tokens[_current].c_str());
	_current++;
	expect(";");
}
ServerConfig ConfigParser::parseServer()
{
	ServerConfig sc;
	expect("server");
	expect("{");
	 while (_current < _tokens.size() && _tokens[_current] != "}")
	{
		if (_tokens[_current] == "host")
			parseHost(sc);
		else if (_tokens[_current] == "listen")
			parseListen(sc);
		else if (_tokens[_current] == "root")
			parseRoot(sc.root);
		else if (_tokens[_current] == "client_max_body_size")
			parseClientMaxBodySize(sc);
		else if (_tokens[_current] == "error_page")
			parseErrorPage(sc);
		else if (_tokens[_current] == "location")
			sc.locations.push_back(parseLocation());
		else
			throw std::runtime_error("Unknown server config: " + _tokens[_current]);
	}
	expect("}");
	validateServer(sc);
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
std::vector<ServerConfig> ConfigParser::parseConfig(const std::string& filename)
{
	std::ifstream conf(filename.c_str());
	if (!conf.is_open())
		throw std::runtime_error("Config file can not be opened");
	_current = 0;
	_tokens.clear();
	tokenize(conf);
	std::vector<ServerConfig> configs;
	while (_current < _tokens.size())
	{
		if (_tokens[_current] != "server")
			throw std::runtime_error("Expected server block");
		configs.push_back(parseServer());
	}
	return configs;
}
