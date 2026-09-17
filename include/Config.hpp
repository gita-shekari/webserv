#pragma once

#include <string>
#include <iostream>
#include <vector>

struct LocationConfig
{
	std::string path;
	std::vector<std::string> methods;
	std::string root;
	std::string index;
};

struct ServerConfig
{
	int port = 0;
	std::string root;
	std::string index;
	std::string errorPage;
	size_t		clientMaxBodySize = 1024 * 1024; // initialize in case config file don't provide this
	std::vector<LocationConfig> locations;
};
