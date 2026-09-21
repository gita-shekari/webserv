#pragma once

#include <string>
#include <iostream>
#include <vector>

const size_t DEFAULT_BODY_SIZE = 1024 * 1024;
const size_t ABS_MAX_CAP_BODY_SIZE = 100 * 1024 * 1024; // 100M safty; for client_max_size = 0M or too big size

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
	std::string error_page;
	size_t		client_max_body_size = DEFAULT_BODY_SIZE; // initialize in case config file don't provide this
	std::vector<LocationConfig> locations;
};
