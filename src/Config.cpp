#include "Config.hpp"

#include <sstream>

void Config::tokenization(char *configFilePath)
{
	std::ifstream file(configFilePath);  

	if (!file.is_open())
	{
		throw ConfigError("Failed to open configuraiton file");
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string configContent = buffer.str();


	size_t pos = 0;
	std::string	delimiters = " \n\t\r";

	while (pos < configContent.size())
	{
		size_t start = configContent.find_first_not_of(delimiters, pos);
		if (start == std::string::npos)
			break;

		if (configContent[start] == '#')
		{
			pos = configContent.find('\n', start);
			if (pos == std::string::npos)
				break;
			continue;
		}

		struct Token token;
		switch (configContent[start])
		{
		case '{':
			token.type = L_BRACE;
			break;
		case '}':
			token.type = R_BRACE;
			break;
		case ';':
			token.type = COLUM;
		default:
			token.type = WORD;
			break;
		}

		size_t end = configContent.find_first_of(delimiters, start);

		if (end == std::string::npos)
		{
			end = configContent.size();
		}
		if (configContent[end - 1] == ';' && end - 1 != start)
		{
			--end;
		}

		token.value = configContent.substr(start, end - start);
		_tokens.push_back(token);
		
		pos = end;
	}

	file.close();
}


const char* Config::ConfigError::what() const noexcept
{
	return _message.c_str();
}