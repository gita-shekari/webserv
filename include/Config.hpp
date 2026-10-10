#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <map>

#include <map>

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
	size_t client_max_body_size = DEFAULT_BODY_SIZE;

	std::map<int, std::string> error_pages;
	std::vector<LocationConfig> locations;
};
