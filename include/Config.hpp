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
	int port;
	std::string root;
	std::string index;
	std::vector<LocationConfig> locations;
};
