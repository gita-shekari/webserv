#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

struct ServerConfig
{
	std::string	host = "";
	int			port = 0;
	std::string	serverName = "localhost";
	//std::string	root = "./www";
	//std::string	index = "index.html";
};


class Config
{
};


#endif
