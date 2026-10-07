#include "Client.hpp"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
// CHANGE: new includes. unistd = unlink(), fstream/sstream = upload file + number to string, ctime = time(), cerrno = errno
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cerrno>
#include <iostream>

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

// CHANGE: new helper. true if the url has a ".." part, so /../../etc/passwd can not leave the root folder
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

// CHANGE: new. CGI stub (not priority). It answers 501 so everything else can be tested.
// Later put the real CGI code here and remove the 501 line.
void Client::handleCGI(const ServerConfig& serverConfig, const std::string& scriptPath)
{
	std::cout << "CGI not implemented yet: " << scriptPath << std::endl;
	// TODO CGI: run the script and build _response from its output
	_response = _builder.buildErrorResponse(501, serverConfig);
}

// CHANGE: new. serve a regular file with status 200. The builder reads the file and builds the response.
void Client::handleStatic(const ServerConfig& serverConfig, const std::string& filePath)
{
	std::cout << "static file: " << filePath << std::endl;
	_response = _builder.buildStaticResponse(filePath, serverConfig);
}

// CHANGE: new. build a redirect response (301 for the missing "/" at the end of a directory url)
void Client::handleRedirect(int code, const std::string& location)
{
	std::cout << "redirect " << code << " to " << location << std::endl;
	_response = _builder.buildRedirectResponse(code, location);
}

// CHANGE: new. DELETE on a regular file: unlink it, 204 if ok, error response if not
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
		return ;
	}
	_response = _builder.buildNoContentResponse();
}

// CHANGE: new. simple upload: the raw body is saved as a new file inside lc.upload_store (no multipart parsing yet)
void Client::handleUpload(const ServerConfig& serverConfig, const LocationConfig& lc)
{
	static unsigned long counter = 0;
	std::string dir = lc.upload_store;
	if (dir[dir.size() - 1] != '/')
		dir += '/';

	std::ostringstream name;
	name << "upload_" << time(NULL) << "_" << counter++;
	std::string target = dir + name.str();

	std::ofstream out(target.c_str(), std::ios::binary);
	if (!out.is_open())
	{
		if (errno == EACCES)
			_request.httpStatus = FORBIDDEN;
		else
			_request.httpStatus = INTERNAL_SERVER_ERR;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return ;
	}
	out.write(_request.body.c_str(), _request.body.size());
	out.close();
	if (out.fail())
	{
		_request.httpStatus = INTERNAL_SERVER_ERR;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return ;
	}
	std::cout << "uploaded to " << target << std::endl;
	_response = _builder.buildCreatedResponse(_request.path + name.str());
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
					_request.httpStatus = INTERNAL_SERVER_ERR;
					if (closedir(dir) == -1)
					{
						std::cout << "closedir failed at " << fullPath << std::endl;
						//Logger::error("closedir failed for " + fullPath + ": " + std::strerror(closeErr));
					};
					// CHANGE: build the error response (before it only returned and the client waited forever)
					_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
					return ;
				}
				break;
			}
			std::string name(ent->d_name);
			if (!name.empty() && name[0] == '.') // skip hidden files.
				continue;
			// CHANGE: add "/" after folder names so the listing shows which entries are folders
			struct stat entBuf;
			if (stat((fullPath + name).c_str(), &entBuf) == 0 && S_ISDIR(entBuf.st_mode))
				name += "/";
			list.push_back(name);
		}
		if (closedir(dir) == -1)
			std::cout << "closedir failed at " << fullPath << std::endl; // change to logger
		// CHANGE: sort the names, readdir order is random
		std::sort(list.begin(), list.end());
		std::cout << "Listing Directory" << std::endl;
		for (size_t i = 0; i < list.size(); i++)
			std::cout << list[i] << std::endl;
		// CHANGE: build the html listing response (was a commented TODO). Needs the url path for the links.
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
		// CHANGE: build the error response (was a commented TODO)
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return ;
	}
}

void Client::handleDirectory(const ServerConfig& serverConfig, std::string& fullPath)
{
	// CHANGE: reference instead of copy, and lowercase the method only one time
	const LocationConfig& lc = serverConfig.locations[_locationIndex];
	std::string method = toLower(_request.method);

	if (method == "get")
	{
		if (!lc.index.empty())
		{
			std::string indexPath = fullPath + lc.index;
			// CHANGE: only use the index if the file really exists. If not, go on to autoindex / 403 below.
			struct stat idxBuf;
			if (stat(indexPath.c_str(), &idxBuf) == 0 && S_ISREG(idxBuf.st_mode))
			{
				if (isCGI(serverConfig, indexPath))
				{
					std::cout << "in handle directory: is CGI going to run cgi" << std::endl;
					handleCGI(serverConfig, indexPath);
				}
				else
				{
					std::cout << "in handle direcotyr: not CGI going to run static" << std::endl;
					handleStatic(serverConfig, indexPath);
				}
				return ;
			}
		}
		// index is empty (or index file not found)
		if (!lc.autoindex)
		{
			_request.httpStatus = FORBIDDEN;
			_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
			std::cout << "location config has no index and no autoindex" << std::endl;
			return ;
		}
		else
			handleDirectoryListing(serverConfig, fullPath);
		// CHANGE: return so GET can not fall into the POST / DELETE checks below
		return ;
	}

	// if req method is POST
	if (method == "post")
	{
		if (!lc.index.empty() && isCGI(serverConfig, fullPath + lc.index))
		{
			std::cout << "POST: index is cgi " << std::endl;
			handleCGI(serverConfig, fullPath + lc.index);
			return ;
		}
		if (!lc.upload_store.empty())
		{
			handleUpload(serverConfig, lc);
			return ;
		}
		else
		{
			_request.httpStatus = FORBIDDEN;
			_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
			return ;
		}
	}
	if (method == "delete")
	{
		_request.httpStatus = FORBIDDEN;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return ;
	}
	// CHANGE: new. method is allowed in the config but the server does not handle it -> 501 Not Implemented
	_response = _builder.buildErrorResponse(501, serverConfig);
}

void Client::routing(const ServerConfig& serverConfig)
{
	// CHANGE: new. check _locationIndex before using it, so locations[] is never read out of range
	if (static_cast<size_t>(_locationIndex) >= serverConfig.locations.size())
	{
		if (_request.httpStatus == REQ_OK)
			_request.httpStatus = INTERNAL_SERVER_ERR;
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		return ;
	}
	std::string root = serverConfig.root;
	const LocationConfig& lc = serverConfig.locations[_locationIndex];
	if (lc.root.size() != 0)
		root = lc.root;
	std::string uriPath = _request.path.substr(0, _request.path.find('?'));
	std::string filesystemPath = root + uriPath;
	std::cout << "file path: " + filesystemPath << std::endl;
	struct stat buf;
	// if (_request.httpStatus == REQ_OK)
	// {
	// 	std::string method = toLower(_request.method);
	// 	bool allowed = false;
	// 	for (size_t i = 0; i < lc.methods.size(); i++)
	// 	{
	// 		if (toLower(lc.methods[i]) == method)
	// 		{
	// 			allowed = true;
	// 			break;
	// 		}
	// 	}
	// 	if (!allowed)
	// 		_request.httpStatus = METHODE_NOT_ALLOWED;
		else if (hasDotDot(uriPath))
			_request.httpStatus = FORBIDDEN;
	}
	std::cout << _request.httpStatus << std::endl;
	if(_request.httpStatus != REQ_OK || !isURIAllowed(serverConfig, &buf, filesystemPath))
	{
		_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		std::cout << "http status has errors" << std::endl;
		return ;
	}

	if (S_ISDIR(buf.st_mode))
	{
		if (!uriPath.empty() && uriPath[uriPath.size() - 1] == '/')
		{
			return handleDirectory(serverConfig, filesystemPath);// handle directory
		}
		else
		{
			return handleRedirect(301, uriPath + '/');
		}
	}
	if (S_ISREG(buf.st_mode))
	{
		if (isCGI(serverConfig, uriPath))
		{
			std::cout << "is CGI going to run cgi" << std::endl;
			handleCGI(serverConfig, filesystemPath);
		}
		else
		{
			std::string method = toLower(_request.method);
			if (method == "get")
			{
				std::cout << " is static going ot run static" << std::endl;
				handleStatic(serverConfig, filesystemPath);
			}
			else if (method == "delete")
				handleDelete(serverConfig, filesystemPath);
			else
			{
				_request.httpStatus = METHODE_NOT_ALLOWED;
				_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
			}
		}
	}
}
