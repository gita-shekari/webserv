#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

// helper

bool isMethodAllowed(const LocationConfig& lc, const std::string& method)
{
	std::vector<std::string>::const_iterator it = std::find(lc.methods.begin(), lc.methods.end(), method);
	if (it == lc.methods.end())
		return false;
	return true;
}
std::string joinPath(const std::string& root, const std::string& path)
{
	if (root.empty())
		return path;
	if (path.empty())
		return root;
	
	bool rootEndsSlash = root[root.size() - 1] == '/';
	bool pathStartsSlash = path[0] == '/';

	if (rootEndsSlash && pathStartsSlash)
		return root + path.substr(1);
	if (!rootEndsSlash && !pathStartsSlash)
		return root + "/" + path;
	return root + path;
}

std::string getCorrectFullPath(const ServerConfig& sc, const LocationConfig& lc, std::string reqPath)
{
	std::string root = sc.root;
	if (lc.root.size() != 0)
		root = lc.root;
	return joinPath(root, reqPath);
}

bool isCGI(const LocationConfig& lc, const std::string& path)
{
	std::string ext;
	size_t dot = path.find_last_of('.');
	size_t slash = path.find_last_of('/');
	
	if (dot == std::string::npos)
		return false;
	if (slash != std::string::npos && slash > dot)
		return false;
	
	ext = path.substr(dot);

	if (lc.cgiHandlers.find(ext) != lc.cgiHandlers.end())
		return true;
	return false;
}

bool Client::isURIAllowed(struct stat* buf, std::string path)
{
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
					int saveErr = errno;
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
		for (int i = 0; i < list.size(); i++)
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

void Client::routingGet(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	struct stat buf;

	if (!isURIAllowed(&buf, fullPath))
	{
		std::cout << "http status has errors" << std::endl;
		return ;
	}

	if (S_ISDIR(buf.st_mode))
	{
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] != '/')
		{
			std::cout << "redirect get directory no /: code 301" << std::endl;
			return ; //handleRedirect(301, _request.path + '/', serverConfig)// handle redirect and add / to the end;
		}
		if (!lc.index.empty())
		{
			if (isCGI(lc, joinPath(fullPath, lc.index)))
			{
				std::cout << joinPath(fullPath, lc.index) << "handleCGI(joinPath(fullPath, lc.index))" << std::endl;
				return ;//handleCGI();	
			}
			std::cout << "lc index, handleStatic " << std::endl;
			return ; //handleStatic();
		}
		if (lc.autoindex)
		{
			std::cout << "dir, no index, autoindex on: handleDirListing()"<< std::endl;
			return ; // handleDirListing(fullPath);
		}
		_request.httpStatus = FORBIDDEN;
		return ;
	}

	if (S_ISREG(buf.st_mode))
	{
		if (isCGI(lc, fullPath))
		{
			std::cout << "regular get, is cgi. handleCGI" << std::endl;
			return ; //handleCGI();
		}
		std::cout << "regular not cgi. handleStacit" << std::endl;
		return ; // handleStatic();
	}
}

void Client::routingPost(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	if (isCGI(lc, fullPath))
	{
		std::cout << "post, is cgi. handleCGI()" << std::endl;
		return ; // handleCGI();
	}


	struct stat buf;
	if (!isURIAllowed(&buf, fullPath))
	{
		std::cout << "post, no upload store, not cgi, not dir or reg." << std::endl;
		return ; 
	}

	if (S_ISDIR(buf.st_mode))
	{
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] != '/')
		{
			std::cout << "redirect post directory no /: code 307" << std::endl;
			return ; //handleRedirect(307, _request.path + '/', serverConfig)// handle redirect and add / to the end;
		}
		if (!lc.index.empty())
		{
			if (isCGI(lc, joinPath(fullPath, lc.index)))
			{
				std::cout << "post, path is direc, add index is cgi. handleCGI()" << std::endl;
				return ; // handleCGI();
			}
		}
	}

	if (!lc.upload_store.empty())
	{
		std::cout << "post, upload to a store. handleUpload()" << std::endl;
		return ; // handleUpload();
	}

	_request.httpStatus = FORBIDDEN;
	return ;

}

void Client::routingDelete(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	struct stat buf;

	if (!isURIAllowed(&buf, fullPath))
	{
		std::cout << "delete, url not allowed" << std::endl;
		return ;
	}

	if (S_ISDIR(buf.st_mode))
	{
		_request.httpStatus = FORBIDDEN;
		std::cout << "delete, paht is dir, forbidden" << std::endl;
		return ;
	}

	std::cout << "delete, regular handleDelete" << std::endl;
	return ; // handleDelete();
}

void Client::routing(const ServerConfig& serverConfig)
{
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	if (_request.httpStatus != REQ_OK)
		return ;

	if (lc.redirectEnabled)
		return ; //handleRedirect(lc.redirectStatus, lc.redirectTarget, serverConfig);

	if (!isMethodAllowed(lc, _request.method))
	{
		std::cout << "method not allowed: METHODE_NOT_ALLOWED" << std::endl;
		_request.httpStatus = METHODE_NOT_ALLOWED;
		return ;
	}

	std::string fullPath = getCorrectFullPath(serverConfig, lc, _request.path);

	if (_request.method == "GET")
		return routingGet(serverConfig, lc, fullPath);
	
	if (_request.method == "POST")
		return routingPost(serverConfig, lc, fullPath);
	
	if (_request.method == "DELETE")
		return routingDelete(serverConfig, lc, fullPath);
}