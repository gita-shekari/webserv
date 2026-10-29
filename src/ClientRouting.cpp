#include "Client.hpp"
#include "RoutingUtils.hpp"

/**
 * @brief Checks whether a requested filesystem path can be handled by the server.
 *
 * Uses stat() to check whether the path exists and to get information
 * about the filesystem object.
 *
 * If stat() fails:
 * - ENOENT / ENOTDIR -> 404 Not Found
 * - EACCES            -> 403 Forbidden
 * - other errors      -> 500 Internal Server Error
 *
 * If stat() succeeds, only regular files and directories are allowed.
 * Other filesystem object types are rejected with 403 Forbidden.
 *
 * The provided struct stat is filled by stat() and can later be used
 * to check whether the path is a file or a directory.
 *
 * @param buf   Pointer to a struct stat that will be filled by stat()
 * @param path path to check
 *
 * @return true if the path is a regular file or directory, otherwise false.
 */
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
	_state = RESPONSE_READY;
}
/**
 * @brief Checks whether an entry inside a directory is itself a directory.
 *
 * Builds the complete filesystem path of the entry and uses stat()
 * to retrieve its metadata.
 *
 * S_ISDIR(info.st_mode) checks whether the file type stored in st_mode
 * represents a directory.
 *
 * @param fullPath
 * @param name      Name of the entry inside that directory.
 *
 * @return true if the entry is a directory, otherwise false.
 */
bool isDirectoryEntry(const std::string& fullPath, const std::string& name)
{
	std::string entryPath = fullPath;

	if (!entryPath.empty() && entryPath[entryPath.size() - 1] != '/')
		entryPath += '/';
	entryPath += name;
	struct stat info;
	if (stat(entryPath.c_str(), &info) == -1)
		return false;
	return S_ISDIR(info.st_mode);
}
/**
 * @brief Reads the entries of an opened directory.
 *
 * Uses readdir() to read one entry at a time from the directory stream.
 * Hidden entries are skipped when their name starts with '.', including
 * ".", "..", and other hidden files.
 *
 * If an entry is itself a directory, "/" is added only to the displayed
 * name so that directories are clear in the generated autoindex
 * (for example "images/"). This does not rename the real directory.
 *
 * @param dir       Opened directory stream returned by opendir().
 * @param fullPath  Filesystem path of the directory being read.
 * @param entries   Vector where the entry names are stored.
 *
 * @return true if the directory is read successfully, otherwise false.
 */
bool readDirectoryEntries(DIR* dir, const std::string& fullPath, std::vector<std::string>& entries)
{
	while (true)
	{
		errno = 0;
		struct dirent* entry = readdir(dir);
		if (entry == NULL)
		{
			if (errno != 0)
				return false;
			break;
		}
		std::string name(entry->d_name);
		if (!name.empty() && name[0] == '.')
			continue;
		if (isDirectoryEntry(fullPath, name))
			name += "/";
		entries.push_back(name);
	}
	return true;
}

/**
 * @brief Generates an autoindex response for a directory.
 *
 * Opens the directory using opendir(), reads its entries with readdir(),
 * sorts them and passes the result list to ResponseBuilder
 * DIR is an opaque directory-stream type defined by <dirent.h>
 * A DIR* is a handle to an opened directory stream
 * Its internal structure is hidden from the user and should only be used with functions: opendir(), readdir() and closedir()
 *
 * @param serverConfig
 * @param fullPath
 */
void Client::handleDirectoryListing(const ServerConfig& serverConfig, const std::string& fullPath)
{
	Logger::debug("directory listing requested: path=" + fullPath);
	DIR* dir = opendir(fullPath.c_str());
	if (dir == NULL)
	{
		Logger::error("opendir failed: path=" + fullPath);
		int saveErr = errno;
		if (saveErr == EACCES || saveErr == ELOOP)
			handleErrorResponse(serverConfig, FORBIDDEN);
		else if (saveErr == ENOENT || saveErr == ENOTDIR)
			handleErrorResponse(serverConfig, PAGE_NOT_FOUND);
		else
			handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
		return;
	}
	std::vector<std::string> entries;
	if (!readDirectoryEntries(dir, fullPath, entries))
	{
		int readErr = errno;
		if (closedir(dir) == -1)
		{
			int closeErr = errno;
			Logger::error("closedir failed for " + fullPath + ": " + std::strerror(closeErr));
		}
		Logger::error("readdir failed for " + fullPath + ": " + std::strerror(readErr));
		handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
		return;
	}
	if (closedir(dir) == -1)
	{
		int closeErr = errno;
		Logger::error("closedir failed for " + fullPath + ": " + std::strerror(closeErr));
	}
	std::sort(entries.begin(), entries.end());
	Logger::debug("directory listing completed: path=" + fullPath);
	_response = _builder.buildListingResponse(entries, _request.path);
	_state = RESPONSE_READY;
}

void Client::handleStatic(const ServerConfig& serverConfig, const std::string& filePath)
{
	std::cout << "static file: " << filePath << std::endl;
	_response = _builder.buildStaticResponse(filePath, serverConfig);
	_state = RESPONSE_READY;
}

void Client::handleRedirect(int code, const std::string& redirURI)
{
	std::cout << "redirect " << code << " to " << redirURI << std::endl;
	_response = _builder.buildRedirectResponse(code, redirURI);
	_state = RESPONSE_READY;
}
/**
 * @brief Deletes a regular file from the filesystem.
 *
 * Uses unlink() to remove the file at filePath.
 * If unlink() fails, errno is checked and mapped to an HTTP status:
 *
 * - permission / filesystem restriction errors -> 403 Forbidden
 * - missing path errors                       -> 404 Not Found
 * - other errors                              -> 500 Internal Server Error
 *  -EACCES( permission denied), EPERM(operation not permitted), EISDIR(target is a directory), EROFS(filesystem is read-only), EBUSY(resource is busy), ELOOP(too many symbolic-link resolutions)
 *  -ENOENT(file/path does not exist), ENOTDIR(some component of the path that should be a directory is not a directory)
 * If the file is deleted successfully, a 204(NoContentResponse) is built
 *
 * @param serverConfig
 * @param filePath
 */
void Client::handleDelete(const ServerConfig& serverConfig, const std::string& filePath)
{
	if (unlink(filePath.c_str()) == -1)
	{
		const int saveErr = errno;
		Logger::error("delete failed: path=" + filePath + ", errno=" + std::to_string(saveErr));
		if (saveErr == EACCES || saveErr == EPERM
			|| saveErr == EISDIR || saveErr == EROFS
			|| saveErr == EBUSY || saveErr == ELOOP)
			return handleErrorResponse(serverConfig, FORBIDDEN);

		if (saveErr == ENOENT || saveErr == ENOTDIR)
			return handleErrorResponse(serverConfig, PAGE_NOT_FOUND);

		return handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
	}
	Logger::debug("delete successful: path=" + filePath);
	_response = _builder.buildNoContentResponse();
	_state = RESPONSE_READY;

}
/**
 * @brief POST body
   ↓
handleUpload()
   ↓
create unique filename
   ↓
open www/uploads/filename
   ↓
write body
   ↓
success -> 201
failure -> 403 or 500
 *
 * @param serverConfig
 * @param lc
 */
void Client::handleUpload(const ServerConfig& serverConfig, const LocationConfig& lc)
{
	static unsigned long counter = 0;

	std::string dir = lc.upload_store;

	if (!dir.empty() && dir[dir.size() - 1] != '/')
		dir += '/';

	std::ostringstream name;
	name << "upload_" << time(NULL) << "_" << counter++;
	// TODO: research on how can we give a name of the uploaded file.
	// abstract the name of the uploaded file, and add a special serials of number. 

	std::string target = dir + name.str();
	std::ofstream out(target.c_str(), std::ios::out | std::ios::binary);
	if (!out.is_open())
	{
		const int saveErr = errno;
		Logger::error("upload failed to open target: path=" + target + ", error=" + std::string(std::strerror(saveErr)));
		if (saveErr == EACCES)
			_request.httpStatus = FORBIDDEN;
		else
			_request.httpStatus = INTERNAL_SERVER_ERR;

		_response = _builder.buildErrorResponse(_request.httpStatus, serverConfig);
		_state = RESPONSE_READY;
		return;
	}
	out.write(_request.body.c_str(), _request.body.size());
	if (!out.good())
	{
		Logger::error("upload failed while writing: path=" + target);
		out.close();
		_response = _builder.buildErrorResponse(INTERNAL_SERVER_ERR, serverConfig);
		_state = RESPONSE_READY;
		return;
	}
	out.close();
	Logger::debug("upload successful: path=" + target + ", bytes=" + std::to_string(_request.body.size()));
	_response = _builder.buildCreatedResponse(_request.path + name.str());
	_state = RESPONSE_READY;
}
/**
 * @brief Handles a GET request after the matching server and location
 *  The path may represent: a directory, a regular file , an invalid or inaccessible resource
 *
 * @param sc
 * @param lc
 * @param fullPath
 */
void Client::routingGet(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	struct stat buf;

	if (!isURIAllowed(&buf, fullPath))
		return handleErrorResponse(sc, _request.httpStatus);
	if (S_ISDIR(buf.st_mode))
	{
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] != '/')
		{
			Logger::debug("GET directory missing trailing slash: redirecting with 301");
			return handleRedirect(301, _request.path + '/');
		}
		if (!lc.index.empty())
		{
			std::string indexPath = RoutingUtils::joinPath(fullPath, lc.index);
			struct stat indexBuf;
			if (!isURIAllowed(&indexBuf, indexPath))
				return handleErrorResponse(sc, _request.httpStatus);
			if (RoutingUtils::isCGI(lc, indexPath))
			{
				Logger::debug("GET directory index detected as CGI: calling handleCGI");
				return handleCGI(sc, lc, indexPath);
			}
			return handleStatic(sc, indexPath);
		}
		if (lc.autoindex)
		{
			Logger::debug("GET directory: no index configured, autoindex enabled");
			return handleDirectoryListing(sc, fullPath);
		}
		return handleErrorResponse(sc, FORBIDDEN);
	}
	if (S_ISREG(buf.st_mode))
	{
		if (RoutingUtils::isCGI(lc, fullPath))
		{
			Logger::debug("GET regular file detected as CGI: calling handleCGI");
			return handleCGI(sc, lc, fullPath);
		}
		return handleStatic(sc, fullPath);
	}
}
// need to check isURI after joinPath as well. like routingGet
void Client::routingPost(const ServerConfig& sc, const LocationConfig& lc, std::string& fullPath)
{
	struct stat buf;
	bool pathAllowed = isURIAllowed(&buf, fullPath);

	if (pathAllowed && S_ISDIR(buf.st_mode))
	{
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] != '/')
		{
			Logger::debug("redirect post directory no /: code 307");
			return handleRedirect(307, _request.path + '/');
		}
		if (!lc.index.empty())
		{
			if (RoutingUtils::isCGI(lc, RoutingUtils::joinPath(fullPath, lc.index)))
			{
				Logger::debug("post, path is direc, add index is cgi. handleCGI()");
				return handleCGI(sc, lc, RoutingUtils::joinPath(fullPath, lc.index));
			}
		}
	}
	if (pathAllowed && S_ISREG(buf.st_mode))
	{
		if (RoutingUtils::isCGI(lc, fullPath))
		{
			std::cout << "post, is cgi. handleCGI()" << std::endl;
			return handleCGI(sc, lc, fullPath);
		}
	}
	if (lc.upload_enabled && !lc.upload_store.empty())
	{
		std::cout << "post, upload to a store. handleUpload()" << std::endl;
		return handleUpload(sc, lc);
	}

	if (!pathAllowed)
		return handleErrorResponse(sc, _request.httpStatus);

	return handleErrorResponse(sc, FORBIDDEN);
}
/**
 * @brief checks if URI is allowed and then if it's a directory. only files can be deleted
 *
 * @param sc
 * @param fullPath
 */
void Client::routingDelete(const ServerConfig& sc, std::string& fullPath)
{
	struct stat buf;

	if (!isURIAllowed(&buf, fullPath))
	{
		Logger::debug("delete failed: resource is not accessible");
		return handleErrorResponse(sc, _request.httpStatus);
	}

	if (S_ISDIR(buf.st_mode))
	{
		Logger::debug("delete forbidden: target is a directory, path=" + fullPath);
		return handleErrorResponse(sc, FORBIDDEN);
	}

	Logger::debug("delete request accepted: deleting regular file, path=" + fullPath);
	return handleDelete(sc, fullPath);
}

void Client::routing(const ServerConfig& serverConfig)
{
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	if (_request.httpStatus != REQ_OK)
		return handleErrorResponse(serverConfig, _request.httpStatus);

	if (lc.has_redirect)
		return handleRedirect(lc.redirect_code, lc.redirect_target);

	if (!RoutingUtils::isMethodAllowed(lc, _request.method))
		return handleErrorResponse(serverConfig, METHODE_NOT_ALLOWED);

	std::string fullPath = RoutingUtils::getCorrectFullPath(serverConfig, lc, _request.path);

	if (_request.method == "GET")
		return routingGet(serverConfig, lc, fullPath);

	if (_request.method == "POST")
		return routingPost(serverConfig, lc, fullPath);

	if (_request.method == "DELETE")
		return routingDelete(serverConfig, fullPath);
}
