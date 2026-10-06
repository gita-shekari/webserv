#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

// helper
std::string	toLower(const std::string& str)
{
	std::string	ret = str;
	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = std::tolower(static_cast<unsigned char>(ret[i]));
	return ret;
}
bool isCGI(const ServerConfig& serverConfig, const std::string& path)
{
	(void)serverConfig;
	size_t pos = path.find_last_of('.');

	if (pos == std::string::npos)
		return false;
	std::string ext = path.substr(pos + 1);
	if (ext == "py" || ext == "php")
		return true;
	return false;
}

// this function is a client class util
bool Client::isURIAllowed(const ServerConfig& serverConfig, struct stat* buf, std::string path)
{
	(void)serverConfig;

	if (stat(path.c_str(), buf) == -1)
	{
		int	saveErr = errno;
		if (saveErr == ENOENT || saveErr == ENOTDIR)
		{
			_request.httpStatus = PAGE_NOT_FOUND;
			std::cout << "page not found" << std::endl;
		}
		else if (saveErr == EACCES)
		{
			_request.httpStatus = FORBIDDEN;
			std::cout << "access forbidden" << std::endl;
		}
		else
		{
			_request.httpStatus = INTERNAL_SERVER_ERR;
			std::cout << "others internal error" << std::endl;
		}
		return false;
	}
	if (!S_ISREG(buf->st_mode) && !S_ISDIR(buf->st_mode))
	{
		_request.httpStatus = FORBIDDEN;
		std::cout << "stat success but not dir or reg" << std::endl;
		return false;
	}
	return true;
}

void Client::handleDirectoryListing(const ServerConfig& serverConfig, std::string& fullPath)
{
	(void)serverConfig;
	DIR *dir = opendir(fullPath.c_str());

	if (dir != NULL)
	{
		std::vector<std::string> list;
		std::cout << "before readdir loop \n" << std::endl;
		struct dirent *ent;
		while(true)
		{
			errno = 0;
			ent = readdir(dir);
			if (ent == NULL)
			{
				if (errno != 0)
				{
					_request.httpStatus = INTERNAL_SERVER_ERR;
					if (closedir(dir) == -1)
					{
						std::cout << "closedir failed at " << fullPath << std::endl;
						//Logger::error("closedir failed for " + fullPath + ": " + std::strerror(closeErr));
					};
					return ; //_builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
				}
				break;
			}
			std::string name(ent->d_name);
			if (!name.empty() && name[0] == '.') // skip hidden files.
				continue;
			list.push_back(name);
		}
		if (closedir(dir) == -1)
			std::cout << "closedir failed at " << fullPath << std::endl; // change to logger
		std::cout << "Listing Directory" << std::endl;
		for (size_t i = 0; i < list.size(); i++)
			std::cout << list[i] << std::endl;
		// buildListingResponse(list);
	}
	else
	{
		int saveErr = errno;
		if (saveErr == EACCES || saveErr == ELOOP)
			_request.httpStatus = FORBIDDEN;
		else if (saveErr == ENOENT || saveErr == ENOTDIR)
			_request.httpStatus = PAGE_NOT_FOUND;
		else
			_request.httpStatus = INTERNAL_SERVER_ERR;
		std::cout << "opendir erro. status code = " << _request.httpStatus << std::endl;
		return ; // buildErrorRepsonse();
	}
}

void Client::handleDirectory(const ServerConfig& serverConfig, std::string& fullPath)
{
	const LocationConfig lc = serverConfig.locations[_locationIndex];

	if (toLower(_request.method) == "get")
	{
		if (!lc.index.empty())
		{
			if (isCGI(serverConfig, fullPath + lc.index))
			{
				std::cout << "in handle directory: is CGI going to run cgi" << std::endl;
				return ;// handleCGI();
			}
			else
			{
				std::cout << "in handle direcotyr: not CGI going to run static" << std::endl;
				return ;//handleStatic()
			}
		}
		// index is empty
		if (!lc.autoindex)
		{
			_request.httpStatus = FORBIDDEN;
			//_builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
			std::cout << "location config has no index and no autoindex" << std::endl;
			return ;
		}
		else
			handleDirectoryListing(serverConfig, fullPath);
	}

	// if req method is POST
	if (toLower(_request.method) == "post")
	{
		if (!lc.index.empty() && isCGI(serverConfig, fullPath + lc.index))
		{
			std::cout << "POST: index is cgi " << std::endl;
			return ; // handleCGI();
		}
		if (!lc.upload_store.empty())
		{
			return ;//handleUpload();
		}
		else
		{
			_request.httpStatus = FORBIDDEN;
			return ; // buildErrorReponse();
		}
	}
	// if req method is DELETE
	if (toLower(_request.method) == "delete")
	{
		_request.httpStatus = FORBIDDEN;
		return ; // buildErrorReponse();
	}
}
void Client::routing(const ServerConfig& serverConfig)
{
	std::string root = serverConfig.root;
	const LocationConfig& lc =
		serverConfig.locations[_locationIndex];

	if (!lc.root.empty())
		root = lc.root;

	std::string filesystemPath = root + _request.path;

	std::cout << "filesystem path: "
			  << filesystemPath << std::endl;


	// -----------------------------------------
	// 1. CHECK METHOD
	// -----------------------------------------

	std::vector<std::string>::const_iterator it =
		std::find(
			lc.methods.begin(),
			lc.methods.end(),
			toLower(_request.method)
		);

	if (it == lc.methods.end())
		_request.httpStatus = METHODE_NOT_ALLOWED;


	// -----------------------------------------
	// 2. CHECK EXISTING ERROR / FILESYSTEM
	// -----------------------------------------

	struct stat buf;

	if (_request.httpStatus != REQ_OK
		|| !isURIAllowed(serverConfig, &buf, filesystemPath))
	{
		_response = _builder.buildErrorResponse(
			static_cast<int>(_request.httpStatus),
			serverConfig
		);

		std::cout
			<< "Error response: "
			<< _response.statusCode
			<< " "
			<< _response.reasonPhrase
			<< std::endl;

		return;
	}


	// =========================================
	// TEMPORARILY COMMENTED WHILE TESTING
	// ERROR RESPONSES
	// =========================================

	/*
	if (S_ISDIR(buf.st_mode))
	{
		if (!_request.path.empty()
			&& _request.path[_request.path.size() - 1] == '/')
		{
			return handleDirectory(
				serverConfig,
				filesystemPath
			);
		}
		else
		{
			// TODO: redirect 301
			return;
		}
	}

	if (S_ISREG(buf.st_mode))
	{
		if (isCGI(serverConfig, filesystemPath))
		{
			// TODO: handleCGI()
		}
		else
		{
			// TODO: handleStatic()
		}
	}
	*/
}
