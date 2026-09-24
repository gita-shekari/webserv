#pragma once

#include <string>
#include <iostream>
#include <vector>
struct LocationConfig
{
	std::string					path;
	std::vector<std::string>	methods;
	std::string					root;
	std::string					index;
	std::string					upload_path;
	size_t						client_max_body_size;
	bool						has_client_max_body_size;
};

struct ServerConfig
{
	int							port;
	std::string					root;
	size_t						client_max_body_size;
	std::map<int, std::string>	error_pages;
	std::vector<LocationConfig>	locations;
};
