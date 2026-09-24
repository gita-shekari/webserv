#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <map>

struct LocationConfig
{
	// Route
	std::string					path;

	// Access
	std::vector<std::string>	methods;

	// Static files
	std::string					root;
	std::string					index;
	bool						autoindex;

	// Request limits
	size_t						client_max_body_size;
	bool						has_client_max_body_size;

	// Upload
	bool						upload_enabled;
	std::string					upload_store;

	// Redirect
	bool						has_redirect;
	int							redirect_code;
	std::string					redirect_target;

	// CGI
	std::string					cgi_extension;
	std::string					cgi_path;
};

struct ServerConfig
{
	std::string					host;
	int							port;
	std::string					root;
	size_t						client_max_body_size;
	std::map<int, std::string>	error_pages;
	std::vector<LocationConfig>	locations;
};
