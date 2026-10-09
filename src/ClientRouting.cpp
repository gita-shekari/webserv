#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cerrno>
#include <iostream>

std::string toLower(const std::string& str)
{
	std::string ret = str;
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

static bool hasDotDot(const std::string& path)
{
	size_t start = 0;

	while (start <= path.size())
	{
		size_t end = path.find('/', start);

		if (end == std::string::npos)
			end = path.size();

		if (path.substr(start, end - start) == "..")
			return true;

		start = end + 1;
	}

	return false;
}

bool Client::isURIAllowed(const ServerConfig& serverConfig, struct stat* buf, std::string path)
{
	(void)serverConfig;

	if (stat(path.c_str(), buf) == -1)
	{
		int saveErr = errno;

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

void Client::handleRedirect(int code, const std::string& location)
{
	std::cout << "redirect " << code << " to " << location << std::endl;
	_response = _builder.buildRedirectResponse(code, location);
}

void Client::handleDelete(const ServerConfig& serverConfig, const std::string& filePath)
{
	if (unlink(filePath.c_str()) == -1)
	{
		int saveErr = errno;

		if (saveErr == EACCES || saveErr == EPERM)
			_request.httpStatus = FORBIDDEN;
		else if (saveErr == ENOENT || saveErr == ENOTDIR)
			_request.httpStatus = PAGE_NOT_FOUND;
		else
			_request.httpStatus = INTERNAL_SERVER_ERR;

		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	_response = _builder.buildNoContentResponse();
}

void Client::handleUpload(const ServerConfig& serverConfig, const LocationConfig& lc)
{
	if (lc.upload_store.empty())
	{
		_request.httpStatus = FORBIDDEN;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

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

void Client::handleDirectory(const ServerConfig& serverConfig, std::string& fullPath)
{
	const LocationConfig& lc = serverConfig.locations[_locationIndex];
	std::string method = toLower(_request.method);

	if (method == "get")
	{
		if (!lc.index.empty())
		{
			std::string indexPath = fullPath;

			if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/')
				indexPath += '/';

			indexPath += lc.index;

			struct stat idxBuf;

			if (stat(indexPath.c_str(), &idxBuf) == 0 && S_ISREG(idxBuf.st_mode))
			{
				if (isCGI(serverConfig, indexPath))
					handleCGI(serverConfig, indexPath);
				else
					handleStatic(serverConfig, indexPath);

				return;
			}
		}

		if (lc.autoindex)
		{
			handleDirectoryListing(serverConfig, fullPath);
			return;
		}

		_request.httpStatus = FORBIDDEN;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	if (method == "post")
	{
		if (!lc.index.empty())
		{
			std::string indexPath = fullPath;

			if (!indexPath.empty() && indexPath[indexPath.size() - 1] != '/')
				indexPath += '/';

			indexPath += lc.index;

			if (isCGI(serverConfig, indexPath))
			{
				handleCGI(serverConfig, indexPath);
				return;
			}
		}

		if (!lc.upload_store.empty())
		{
			handleUpload(serverConfig, lc);
			return;
		}

		_request.httpStatus = FORBIDDEN;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	if (method == "delete")
	{
		_request.httpStatus = FORBIDDEN;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	_response = _builder.buildErrorResponse(501, serverConfig);
}

void Client::routing(const ServerConfig& serverConfig)
{
	if (static_cast<size_t>(_locationIndex) >= serverConfig.locations.size())
	{
		if (_request.httpStatus == REQ_OK)
			_request.httpStatus = INTERNAL_SERVER_ERR;

		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	std::string root = serverConfig.root;
	const LocationConfig& lc = serverConfig.locations[_locationIndex];

	if (!lc.root.empty())
		root = lc.root;

	std::string uriPath = _request.path.substr(0, _request.path.find('?'));
	std::string filesystemPath = root + uriPath;

	std::cout << "file path: " << filesystemPath << std::endl;

	if (_request.httpStatus == REQ_OK)
	{
		std::string method = toLower(_request.method);
		std::vector<std::string>::const_iterator it = std::find(lc.methods.begin(), lc.methods.end(), method);

		if (it == lc.methods.end())
			_request.httpStatus = METHODE_NOT_ALLOWED;
		else if (hasDotDot(uriPath))
			_request.httpStatus = FORBIDDEN;
	}

	if (_request.httpStatus != REQ_OK)
	{
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	struct stat buf;

	if (!isURIAllowed(serverConfig, &buf, filesystemPath))
	{
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	if (S_ISDIR(buf.st_mode))
	{
		if (!uriPath.empty() && uriPath[uriPath.size() - 1] != '/')
		{
			handleRedirect(301, uriPath + "/");
			return;
		}

		handleDirectory(serverConfig, filesystemPath);
		return;
	}

	if (S_ISREG(buf.st_mode))
	{
		if (isCGI(serverConfig, filesystemPath))
		{
			handleCGI(serverConfig, filesystemPath);
			return;
		}

		std::string method = toLower(_request.method);

		if (method == "get")
		{
			handleStatic(serverConfig, filesystemPath);
			return;
		}

		if (method == "delete")
		{
			handleDelete(serverConfig, filesystemPath);
			return;
		}

		_request.httpStatus = METHODE_NOT_ALLOWED;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return;
	}

	_request.httpStatus = INTERNAL_SERVER_ERR;
	_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
}