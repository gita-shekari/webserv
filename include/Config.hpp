#pragma once

#include <string>
#include <iostream>
#include <vector>

const size_t DEFAULT_BODY_SIZE = 1024 * 1024;
const size_t ABS_MAX_CAP_BODY_SIZE = 100 * 1024 * 1024; // 100M safty; for client_max_size = 0M or too big size
const std::string DEFAULT_ERROR_PAGE = "error.html";

struct LocationConfig
{
	std::string path;
	std::vector<std::string> methods;
	std::string root;
	std::string index;
	std::string upload_store;
 	size_t client_max_body_size;
 	bool has_client_max_body_size;
};

struct ServerConfig
{
	int port = 0;
	std::string root;
	size_t client_max_body_size;
	std::map<int, std::string>	error_pages;
	std::vector<LocationConfig>	locations;
};