#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <cerrno>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

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

	if (lc.cgi_extension == ext)
		return true;
	
	//if (lc.cgiHandlers.find(ext) != lc.cgiHandlers.end())
	//	return true;
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


void Client::handleErrorResponse(const ServerConfig& sc, HttpStatus code)
{
	_request.httpStatus = code;
	_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), sc);
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
					if (closedir(dir) == -1)
					{
						std::cout << "closedir failed at " << fullPath << std::endl;
						//Logger::error("closedir failed for " + fullPath + ": " + std::strerror(closeErr));
					};
					return handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
				}
				break;
			}
			std::string name(ent->d_name);
			if (!name.empty() && name[0] == '.') // skip hidden files.
				continue;

			//what is this?
			struct stat entBuf;
			std::string entryPath = fullPath;

			if (!entryPath.empty() && entryPath[entryPath.size() - 1] != '/')
				entryPath += '/';

			entryPath += name;

			if (stat(entryPath.c_str(), &entBuf) == 0 && S_ISDIR(entBuf.st_mode))
				name += "/";
			///

			list.push_back(name);
		}
		if (closedir(dir) == -1)
			std::cout << "closedir failed at " << fullPath << std::endl; // change to logger
		std::sort(list.begin(), list.end());
		std::cout << "Listing Directory" << std::endl;
		for (size_t i = 0; i < list.size(); i++)
			std::cout << list[i] << std::endl;
		_response = _builder.buildListingResponse(list, _request.path);
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
		std::cout << "saveErr " << saveErr << std::endl;
		handleErrorResponse(serverConfig, _request.httpStatus);
	}
}

void Client::handleCGI(const ServerConfig& serverConfig, const std::string& scriptPath)
{
	std::cout << "CGI not implemented yet: " << scriptPath << std::endl;
	_response = _builder.buildErrorResponse(501, serverConfig);
}

void Client::handleStatic(const ServerConfig& serverConfig, const std::string& filePath)
{
	std::cout << "static file: " << filePath << std::endl;
	_response = _builder.buildStaticResponse(filePath, serverConfig);
}

void Client::handleRedirect(int code, const std::string& redirURI)
{
	std::cout << "redirect " << code << " to " << redirURI << std::endl;
	_response = _builder.buildRedirectResponse(code, redirURI);
}

void Client::handleDelete(const ServerConfig& serverConfig, const std::string& filePath)
{
	if (unlink(filePath.c_str()) == -1)
	{
		const int saveErr = errno;

		if (saveErr == EACCES || saveErr == EPERM
			|| saveErr == EISDIR || saveErr == EROFS
			|| saveErr == EBUSY || saveErr == ELOOP)
			return handleErrorResponse(serverConfig, FORBIDDEN);

		if (saveErr == ENOENT || saveErr == ENOTDIR)
			return handleErrorResponse(serverConfig, PAGE_NOT_FOUND);

		return handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
	}

	_response = _builder.buildNoContentResponse();
}

void Client::handleUpload(const ServerConfig& serverConfig, const LocationConfig& lc)
{
	//if (lc.upload_store.empty())
	//{
	//	_request.httpStatus = FORBIDDEN;
	//	_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
	//	return;
	//}

	static unsigned long counter = 0;

	std::string dir = lc.upload_store;

	if (!dir.empty() && dir[dir.size() - 1] != '/')
		dir += '/';

	std::ostringstream name;
	name << "upload_" << time(NULL) << "_" << counter++;

	std::string target = dir + name.str();

	std::ofstream out(target.c_str(), std::ios::out | std::ios::binary);

	if (!out.is_open())
	{
		if (errno == EACCES)
			_request.httpStatus = FORBIDDEN;
		else
			_request.httpStatus = INTERNAL_SERVER_ERR;

		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	out.write(_request.body.c_str(), _request.body.size());

	if (!out.good())
	{
		out.close();
		_request.httpStatus = INTERNAL_SERVER_ERR;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	out.close();

	std::cout << "uploaded to " << target << std::endl;

	_response = _builder.buildCreatedResponse(_request.path + name.str());
}

void Client::handleDirectoryListing(const ServerConfig& serverConfig, std::string& fullPath)
{
	DIR* dir = opendir(fullPath.c_str());

	if (dir != NULL)
	{
		std::vector<std::string> list;
		struct dirent* ent;

		while (true)
		{
			errno = 0;
			ent = readdir(dir);

			if (ent == NULL)
			{
				if (errno != 0)
				{
					_request.httpStatus = INTERNAL_SERVER_ERR;

					if (closedir(dir) == -1)
						std::cout << "closedir failed at " << fullPath << std::endl;

					_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
					return;
				}

				break;
			}

			std::string name(ent->d_name);

			if (!name.empty() && name[0] == '.')
				continue;

			struct stat entBuf;
			std::string entryPath = fullPath;

			if (!entryPath.empty() && entryPath[entryPath.size() - 1] != '/')
				entryPath += '/';

			entryPath += name;

			if (stat(entryPath.c_str(), &entBuf) == 0 && S_ISDIR(entBuf.st_mode))
				name += "/";

			list.push_back(name);
		}

		if (closedir(dir) == -1)
			std::cout << "closedir failed at " << fullPath << std::endl;

		std::sort(list.begin(), list.end());

		std::cout << "Listing Directory" << std::endl;

		for (size_t i = 0; i < list.size(); ++i)
			std::cout << list[i] << std::endl;

		_response = _builder.buildListingResponse(list, _request.path);
		return;
	}

	int saveErr = errno;

	if (saveErr == EACCES || saveErr == ELOOP)
		_request.httpStatus = FORBIDDEN;
	else if (saveErr == ENOENT || saveErr == ENOTDIR)
		_request.httpStatus = PAGE_NOT_FOUND;
	else
		_request.httpStatus = INTERNAL_SERVER_ERR;

	_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
}

void Client::routingGet(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	struct stat buf;

	if (!isURIAllowed(&buf, fullPath))
	{
		std::cout << "http status has errors" << std::endl;
		return handleErrorResponse(sc, _request.httpStatus);
	}

	if (S_ISDIR(buf.st_mode))
	{
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] != '/')
		{
			std::cout << "redirect get directory no /: code 301" << std::endl;
			return handleRedirect(301, _request.path + '/');
		}
		if (!lc.index.empty())
		{
			if (isCGI(lc, joinPath(fullPath, lc.index)))
				return handleCGI(sc, joinPath(fullPath, lc.index));	
			std::cout << "lc index, handleStatic " << std::endl;
			return handleStatic(sc, joinPath(fullPath, lc.index));
		}
		if (lc.autoindex)
		{
			std::cout << "dir, no index, autoindex on: handleDirectoryListing()"<< std::endl;
			return handleDirectoryListing(sc, fullPath);
		}
		return handleErrorResponse(sc, FORBIDDEN);
	}

	if (S_ISREG(buf.st_mode))
	{
		if (isCGI(lc, fullPath))
		{
			std::cout << "regular get, is cgi. handleCGI" << std::endl;
			return handleCGI(sc, fullPath);
		}
		std::cout << "regular not cgi. handleStacit" << std::endl;
		return handleStatic(sc, fullPath);
	}
}

void Client::routingPost(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	struct stat buf;
	bool pathAllowed = isURIAllowed(&buf, fullPath);

	if (pathAllowed && S_ISDIR(buf.st_mode))
	{
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] != '/')
		{
			std::cout << "redirect post directory no /: code 307" << std::endl;
			return handleRedirect(307, _request.path + '/');
		}
		if (!lc.index.empty())
		{
			if (isCGI(lc, joinPath(fullPath, lc.index)))
			{
				std::cout << "post, path is direc, add index is cgi. handleCGI()" << std::endl;
				return handleCGI(sc, joinPath(fullPath, lc.index));
			}
		}
	}
	if (pathAllowed && S_ISREG(buf.st_mode))
	{
		if (isCGI(lc, fullPath))
		{
			std::cout << "post, is cgi. handleCGI()" << std::endl;
			return handleCGI(sc, fullPath);
		}
	}
	if (!lc.upload_store.empty())
	{
		std::cout << "post, upload to a store. handleUpload()" << std::endl;
		return handleUpload(sc, lc);
	}

	if (!pathAllowed)
		return handleErrorResponse(sc, _request.httpStatus);
	
	return handleErrorResponse(sc, FORBIDDEN);
}

void Client::routingDelete(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	struct stat buf;

	if (!isURIAllowed(&buf, fullPath))
	{
		std::cout << "delete, url not allowed" << std::endl;
		return handleErrorResponse(sc, _request.httpStatus);
	}

	if (S_ISDIR(buf.st_mode))
	{
		std::cout << "delete, paht is dir, forbidden" << std::endl;
		return handleErrorResponse(sc, FORBIDDEN);
	}

	std::cout << "delete, regular handleDelete" << std::endl;
	return handleDelete(sc, fullPath);
}

void Client::routing(const ServerConfig& serverConfig)
{
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	if (_request.httpStatus != REQ_OK)
		return handleErrorResponse(serverConfig, _request.httpStatus);

	if (lc.has_redirect)
		return handleRedirect(lc.redirect_code, lc.redirect_target);

	if (!isMethodAllowed(lc, _request.method))
	{
		std::cout << "method not allowed: METHODE_NOT_ALLOWED" << std::endl;
		return handleErrorResponse(serverConfig, METHODE_NOT_ALLOWED);
	}

	std::string fullPath = getCorrectFullPath(serverConfig, lc, _request.path);

	if (_request.method == "GET")
		return routingGet(serverConfig, lc, fullPath);
	
	if (_request.method == "POST")
		return routingPost(serverConfig, lc, fullPath);
	
	if (_request.method == "DELETE")
		return routingDelete(serverConfig, lc, fullPath);
}
