#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <map>

struct LocationConfig
{
	std::string path;
	std::vector<std::string> methods;

	std::string root;
	std::string index;
	bool autoindex = false;

	size_t client_max_body_size = 0;
	bool has_client_max_body_size = false;

	bool upload_enabled = false;
	std::string upload_store;

	bool has_redirect = false;
	int redirect_code = 0;
	std::string redirect_target;

	std::string cgi_extension;
	std::string cgi_path;
};

struct ServerConfig
{
	std::string host = "0.0.0.0";
	int port = 0;

	std::string root;
	size_t client_max_body_size = 1024 * 1024;

	std::map<int, std::string> error_pages;
	std::vector<LocationConfig> locations;
};
