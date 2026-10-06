#include <iostream>
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <map>
#include <vector>

enum	HttpStatus
{
	REQ_OK = 200,
	CREATED = 201,
	MOVED_PERMANENTLY = 301,
	BAD_REQ = 400,
	FORBIDDEN = 403,
	PAGE_NOT_FOUND = 404,
	METHODE_NOT_ALLOWED = 405,
	PLAYLOAD_TOO_LARGE = 413,
	INTERNAL_SERVER_ERR = 500,
	NOT_IMPLEMENTED = 501,
	HTTP_VERSION_NOT_NSUP = 505
};

struct Request
{
	std::string	method;
	std::string rawTarget;
	std::string	path;
	std::string	query;
	HttpStatus	httpStatus = REQ_OK;
	std::map<std::string, std::string> headers;
	std::string	body;

	Request(){};
	Request(std::string met, std::string pt, HttpStatus htt) 
		: method(met), path(pt), httpStatus(htt) {}
};

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

	LocationConfig(std::string idx, std::vector<std::string> mets, bool autoidx, std::string path, bool redirEabled, int redirStatus, std::string redirTar, std::string root, std::string up_store)
		: index(idx), methods(mets), autoindex(autoidx), path(path), redirectEnabled(redirEabled), redirectStatus(redirStatus), redirectTarget(redirTar), upload_store(up_store) {}
};

struct ServerConfig
{
	int port = 0;
	std::string root = "";
	std::string index = "";
	std::string error_page;
	std::vector<LocationConfig> locations;
};


class TestClient
{
	public:
		TestClient(size_t lcIdx, Request& req);
		bool isURIAllowed(const ServerConfig& serverConfig, struct stat* buf, std::string path);
		

		void	handleDirectory(const ServerConfig& serverConfig, std::string& fullPath);
		void routing(const ServerConfig& serverConfig);

		void handleDirectoryListing(const ServerConfig& serverConfig, std::string& fullPath);
		// handleRedirect()
		// handleStatic()
		// handleCGI()
	private:
		size_t			_locationIndex = static_cast<size_t>(0);
		Request			_request; //current request
};

TestClient::TestClient(size_t lcIdx, Request& req)
{
	_locationIndex = lcIdx;
	_request = req;
}
// from here is functions that will be needed.
std::string	toLower(const std::string& str)
{
	std::string	ret = str;
	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = std::tolower(static_cast<unsigned char>(ret[i]));
	return ret;
}
// helper
bool isCGI(const ServerConfig& serverConfig, const std::string& path)
{
	std::string ext;
	size_t idx = path.size() - 1;

	while (idx >= 0)
	{
		if (path[idx] == '.')
			break;
		ext += path[idx];
		idx--;
	}
	// TODO: should I make sure idx != 0? because we are sure it won't happen. 
	std::reverse(ext.begin(), ext.end());
	if (ext == "py" || ext == "php")
		return true;
	else 
		return false;
}

// this function is a client class util
bool TestClient::isURIAllowed(const ServerConfig& serverConfig, struct stat* buf, std::string path)
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

void TestClient::handleDirectoryListing(const ServerConfig& serverConfig, std::string& fullPath)
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

void TestClient::handleDirectory(const ServerConfig& serverConfig, std::string& fullPath)
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



void TestClient::routing(const ServerConfig& serverConfig)
{
	std::string root = serverConfig.root;
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	if (lc.root.size() != 0)
		root = lc.root;
	std::string filesystemPath = root + _request.path;
	std::cout << "file path: " + filesystemPath << std::endl;
	struct stat buf;
	std::vector<std::string>::const_iterator it = std::find(lc.methods.begin(), lc.methods.end(), toLower(_request.method));
	if (it == lc.methods.end())
		_request.httpStatus = METHODE_NOT_ALLOWED;
	if(_request.httpStatus != REQ_OK || !isURIAllowed(serverConfig, &buf, filesystemPath))
	{
		//_response = _builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
		std::cout << "http status is has errors" << std::endl;
		return ;
	}			

	if (S_ISDIR(buf.st_mode))
	{
		// TODO: make sure request.path will never be empty()
		if (!_request.path.empty() && _request.path[_request.path.size() - 1] == '/')
		{
			return handleDirectory(serverConfig, filesystemPath);// handle directory
		}
		else
		{
			std::cout << "coming soon: handleRedirect(301)" << std::endl;
			return ; //handleRedirect(301, _request.path + '/', serverConfig)// handle redirect and add / to the end;
		}
	}
	if (S_ISREG(buf.st_mode))
	{
		if (isCGI(serverConfig, _request.path))
		{
			std::cout << "is CGI going to run cgi" << std::endl;
			// handleCGI();
		}
		else
		{
			std::cout << " is static going ot run static" << std::endl;
			// handleStatic();
		}
	}
}


void initServerConfig(struct ServerConfig& sc)
{
	sc.index = "";
	sc.root = "fixtures/www";
}

int main(void)
{
	struct ServerConfig	sc;
	initServerConfig(sc);
	
	// check directory listing 
	struct LocationConfig lc("", {"get"}, true, "/listing", false, -1, "", "", "");
	sc.locations.push_back(lc);
	struct Request req_tailed("get", "/listing/", REQ_OK);
	struct Request req_notailed("get", "/listing", REQ_OK);


	size_t indexToTest = 0;
	TestClient client(indexToTest, req_tailed);
	client.routing(sc);

	TestClient client2(indexToTest, req_notailed);
	client2.routing(sc);

	//index(idx), methods(mets), autoindex(autoidx), path(path), redirectEnabled(redirEabled), redirectStatus(redirStatus), redirectTarget(redirTar), upload_store(up_store)
	struct LocationConfig lc1("index.html", {"get"}, true, "/with_index", false, -1, "", "", "");
	sc.locations.push_back(lc1);
	indexToTest++;
	struct Request req_index("get", "/with_index/", REQ_OK);
	TestClient client3(indexToTest, req_index);
	client3.routing(sc);

}
