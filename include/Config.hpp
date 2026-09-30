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
	size_t 		client_max_body_size; 
	bool 		has_client_max_body_size;

	std::string index;
	bool 		autoindex = false;
	std::string upload_store;
	std::map<std::string, std::string> cgiHandlers;
	bool 		redirectEnabled = false;
	int 		redirectStatus = 0;
	std::string redirectTarget;
};

struct ServerConfig
{
	int port = 0;
	std::string root;
	std::string index;
	std::string error_page = DEFAULT_ERROR_PAGE;
	size_t		client_max_body_size = DEFAULT_BODY_SIZE; // initialize in case config file don't provide this
	std::vector<LocationConfig> locations;
};
