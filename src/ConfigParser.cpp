#include "ConfigParser.hpp"

ConfigParser::ConfigParser() : _current(0)
{
}

/* ************************************************************************** */
/*                               TOKENIZATION                                 */
/* ************************************************************************** */

void ConfigParser::tokenize(std::ifstream& file)
{
	std::string line;

	while (std::getline(file, line))
	{
		std::string token;

		for (size_t i = 0; i < line.size(); i++)
		{
			// comments
			if (line[i] == '#')
				break;

			if (line[i] == '/' &&
				i + 1 < line.size() &&
				line[i + 1] == '/')
				break;

			if (std::isspace(static_cast<unsigned char>(line[i])))
			{
				if (!token.empty())
				{
					_tokens.push_back(token);
					token.clear();
				}
			}
			else if (line[i] == '{' ||
					 line[i] == '}' ||
					 line[i] == ';')
			{
				if (!token.empty())
				{
					_tokens.push_back(token);
					token.clear();
				}

				_tokens.push_back(std::string(1, line[i]));
			}
			else
				token += line[i];
		}

		if (!token.empty())
			_tokens.push_back(token);
	}
}

/* ************************************************************************** */
/*                                VALIDATION                                  */
/* ************************************************************************** */

bool ConfigParser::isValidPort(const std::string& token)
{
	if (token.empty())
		return false;

	for (size_t i = 0; i < token.size(); i++)
	{
		if (!std::isdigit(static_cast<unsigned char>(token[i])))
			return false;
	}

	long port = std::strtol(token.c_str(), NULL, 10);

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
	if (token.empty())
		throw std::runtime_error("Invalid client_max_body_size");

	size_t multiplier = 1;
	std::string number = token;

	char suffix = token[token.size() - 1];

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

	for (size_t i = 0; i < number.size(); i++)
	{
		if (!std::isdigit(static_cast<unsigned char>(number[i])))
			throw std::runtime_error("Invalid client_max_body_size");
	}

	unsigned long long value = std::strtoull(number.c_str(), NULL, 10);

	if (value > std::numeric_limits<size_t>::max() / multiplier)
		throw std::runtime_error("client_max_body_size too large");

	return static_cast<size_t>(value * multiplier);
}

bool ConfigParser::isValidMethod(const std::string& method)
{
	return (method == "GET" ||
			method == "POST" ||
			method == "DELETE");
}

/* ************************************************************************** */
/*                                  EXPECT                                    */
/* ************************************************************************** */

void ConfigParser::expect(const std::string& expected)
{
	if (_current >= _tokens.size())
		throw std::runtime_error(
			"Unexpected end of config, expected '" + expected + "'");

	if (_tokens[_current] != expected)
		throw std::runtime_error(
			"Expected '" + expected +
			"', found '" + _tokens[_current] + "'");

	_current++;
}

/* ************************************************************************** */
/*                            SERVER DIRECTIVES                               */
/* ************************************************************************** */

void ConfigParser::parseListen(ServerConfig& sc)
{
	expect("listen");

	if (_current >= _tokens.size())
		throw std::runtime_error("Missing listen value");

	if (!isValidPort(_tokens[_current]))
		throw std::runtime_error(
			"Invalid port: " + _tokens[_current]);

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
		throw std::runtime_error(
			"Invalid host: " + _tokens[_current]);

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

void ConfigParser::parseClientMaxBodySize(ServerConfig& sc)
{
	expect("client_max_body_size");

	if (_current >= _tokens.size())
		throw std::runtime_error(
			"Missing client_max_body_size value");

	sc.client_max_body_size =
		extract_size(_tokens[_current]);

	_current++;

	expect(";");
}

void ConfigParser::parseErrorPage(ServerConfig& sc)
{
	expect("error_page");

	if (_current >= _tokens.size())
		throw std::runtime_error("Missing error status code");

	const std::string codeToken = _tokens[_current];

	for (size_t i = 0; i < codeToken.size(); i++)
	{
		if (!std::isdigit(
				static_cast<unsigned char>(codeToken[i])))
			throw std::runtime_error(
				"Invalid error status code: " + codeToken);
	}

	int code = std::atoi(codeToken.c_str());

	if (code < 400 || code > 599)
		throw std::runtime_error(
			"Invalid error status code: " + codeToken);

	_current++;

	if (_current >= _tokens.size() ||
		_tokens[_current] == ";" ||
		_tokens[_current] == "}")
		throw std::runtime_error("Missing error page path");

	std::string path = _tokens[_current];

	_current++;

	expect(";");

	sc.error_pages[code] = path;
}

/* ************************************************************************** */
/*                           LOCATION DIRECTIVES                              */
/* ************************************************************************** */

void ConfigParser::parseMethods(LocationConfig& lc)
{
	expect("methods");

	if (_current >= _tokens.size() ||
		_tokens[_current] == ";")
		throw std::runtime_error(
			"At least one method is required");

	while (_current < _tokens.size() &&
		   _tokens[_current] != ";")
	{
		if (!isValidMethod(_tokens[_current]))
			throw std::runtime_error(
				"Invalid HTTP method: " + _tokens[_current]);

		lc.methods.push_back(_tokens[_current]);

		_current++;
	}

	expect(";");
}

void ConfigParser::parseIndex(LocationConfig& lc)
{
	expect("index");

	if (_current >= _tokens.size() ||
		_tokens[_current] == ";" ||
		_tokens[_current] == "}")
		throw std::runtime_error("Missing index file");

	lc.index = _tokens[_current];

	_current++;

	expect(";");
}

void ConfigParser::parseAutoindex(LocationConfig& lc)
{
	expect("autoindex");

	if (_current >= _tokens.size())
		throw std::runtime_error(
			"Missing autoindex value");

	if (_tokens[_current] == "on")
		lc.autoindex = true;
	else if (_tokens[_current] == "off")
		lc.autoindex = false;
	else
		throw std::runtime_error(
			"autoindex must be 'on' or 'off'");

	_current++;

	expect(";");
}

void ConfigParser::parseClientMaxBodySize(LocationConfig& lc)
{
	expect("client_max_body_size");

	if (_current >= _tokens.size())
		throw std::runtime_error(
			"Missing client_max_body_size value");

	lc.client_max_body_size =
		extract_size(_tokens[_current]);

	lc.has_client_max_body_size = true;

	_current++;

	expect(";");
}

void ConfigParser::parseUpload(LocationConfig& lc)
{
	expect("upload");

	if (_current >= _tokens.size())
		throw std::runtime_error("Missing upload value");

	if (_tokens[_current] == "on")
		lc.upload_enabled = true;
	else if (_tokens[_current] == "off")
		lc.upload_enabled = false;
	else
		throw std::runtime_error(
			"upload must be 'on' or 'off'");

	_current++;

	expect(";");
}

void ConfigParser::parseUploadStore(LocationConfig& lc)
{
	expect("upload_store");

	if (_current >= _tokens.size() ||
		_tokens[_current] == ";" ||
		_tokens[_current] == "}")
		throw std::runtime_error(
			"Missing upload_store path");

	lc.upload_store = _tokens[_current];

	_current++;

	expect(";");
}

void ConfigParser::parseRedirect(LocationConfig& lc)
{
	expect("return");

	if (_current >= _tokens.size())
		throw std::runtime_error(
			"Missing redirect status code");

	if (!isValidPort(_tokens[_current]))
		throw std::runtime_error(
			"Invalid redirect status code");

	int code = std::atoi(_tokens[_current].c_str());

	if (code < 300 || code > 399)
		throw std::runtime_error(
			"Redirect status code must be 3xx");

	_current++;

	if (_current >= _tokens.size() ||
		_tokens[_current] == ";" ||
		_tokens[_current] == "}")
		throw std::runtime_error(
			"Missing redirect target");

	lc.has_redirect = true;
	lc.redirect_code = code;
	lc.redirect_target = _tokens[_current];

	_current++;

	expect(";");
}

void ConfigParser::parseCgiExtension(LocationConfig& lc)
{
	expect("cgi_extension");

	if (_current >= _tokens.size() ||
		_tokens[_current] == ";" ||
		_tokens[_current] == "}")
		throw std::runtime_error(
			"Missing CGI extension");

	lc.cgi_extension = _tokens[_current];

	_current++;

	expect(";");
}

void ConfigParser::parseCgiPath(LocationConfig& lc)
{
	expect("cgi_path");

	if (_current >= _tokens.size() ||
		_tokens[_current] == ";" ||
		_tokens[_current] == "}")
		throw std::runtime_error(
			"Missing CGI path");

	lc.cgi_path = _tokens[_current];

	_current++;

	expect(";");
}

/* ************************************************************************** */
/*                           LOCATION VALIDATION                              */
/* ************************************************************************** */

void ConfigParser::validateLocation(const LocationConfig& lc)
{
	if (lc.path.empty())
		throw std::runtime_error(
			"Location path cannot be empty");

	if (lc.path[0] != '/')
		throw std::runtime_error(
			"Location path must start with '/'");

	if (lc.upload_enabled && lc.upload_store.empty())
		throw std::runtime_error(
			"Upload enabled but upload_store is missing");

	if (!lc.cgi_extension.empty() && lc.cgi_path.empty())
		throw std::runtime_error(
			"cgi_extension requires cgi_path");

	if (lc.cgi_extension.empty() && !lc.cgi_path.empty())
		throw std::runtime_error(
			"cgi_path requires cgi_extension");
}

/* ************************************************************************** */
/*                            LOCATION BLOCK                                  */
/* ************************************************************************** */

LocationConfig ConfigParser::parseLocation()
{
	LocationConfig lc;

	expect("location");

	if (_current >= _tokens.size() ||
		_tokens[_current] == "{")
		throw std::runtime_error(
			"Missing location path");

	lc.path = _tokens[_current++];

	expect("{");

	while (_current < _tokens.size() &&
		   _tokens[_current] != "}")
	{
		if (_tokens[_current] == "methods")
			parseMethods(lc);

		else if (_tokens[_current] == "root")
			parseRoot(lc.root);

		else if (_tokens[_current] == "index")
			parseIndex(lc);

		else if (_tokens[_current] == "autoindex")
			parseAutoindex(lc);

		else if (_tokens[_current] ==
				 "client_max_body_size")
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
				"Unknown location directive: " +
				_tokens[_current]);
	}

	expect("}");

	validateLocation(lc);

	return lc;
}

/* ************************************************************************** */
/*                             SERVER VALIDATION                              */
/* ************************************************************************** */

void ConfigParser::validateServer(const ServerConfig& sc)
{
	if (sc.port <= 0)
		throw std::runtime_error(
			"Server listen port is missing");

	if (sc.host.empty())
		throw std::runtime_error(
			"Server host is missing");

	if (sc.root.empty())
		throw std::runtime_error(
			"Server root is missing");

	for (size_t i = 0; i < sc.locations.size(); i++)
	{
		for (size_t j = i + 1;
			 j < sc.locations.size();
			 j++)
		{
			if (sc.locations[i].path ==
				sc.locations[j].path)
				throw std::runtime_error(
					"Duplicate location: " +
					sc.locations[i].path);
		}
	}
}

/* ************************************************************************** */
/*                               SERVER BLOCK                                 */
/* ************************************************************************** */

ServerConfig ConfigParser::parseServer()
{
	ServerConfig sc;

	expect("server");
	expect("{");

	while (_current < _tokens.size() &&
		   _tokens[_current] != "}")
	{
		if (_tokens[_current] == "host")
			parseHost(sc);

		else if (_tokens[_current] == "listen")
			parseListen(sc);

		else if (_tokens[_current] == "root")
			parseRoot(sc.root);

		else if (_tokens[_current] ==
				 "client_max_body_size")
			parseClientMaxBodySize(sc);

		else if (_tokens[_current] == "error_page")
			parseErrorPage(sc);

		else if (_tokens[_current] == "location")
			sc.locations.push_back(parseLocation());

		else
			throw std::runtime_error(
				"Unknown server directive: " +
				_tokens[_current]);
	}

	expect("}");

	validateServer(sc);

	return sc;
}

/* ************************************************************************** */
/*                              CONFIG FILE                                   */
/* ************************************************************************** */

std::vector<ServerConfig> ConfigParser::parseConfig(const std::string& filename)
{
	std::ifstream conf(filename.c_str());

	if (!conf.is_open())
		throw std::runtime_error(
			"Config file cannot be opened: " + filename);

	_current = 0;
	_tokens.clear();

	tokenize(conf);

	if (_tokens.empty())
		throw std::runtime_error(
			"Config file is empty");

	std::vector<ServerConfig> configs;

	while (_current < _tokens.size())
	{
		if (_tokens[_current] != "server")
			throw std::runtime_error(
				"Expected server block, found: " +
				_tokens[_current]);

		configs.push_back(parseServer());
	}

	return configs;
}
