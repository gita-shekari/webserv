#pragma once

# include <string>
# include <map>

struct Response
{
	std::string							version;
	int									statusCode;
	std::string							reasonPhrase;
	std::map<std::string, std::string>	headers;
	std::string							body;
};
