#include <iostream>
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <map>
#include <vector>

enum	HttpStatus
{
	REQ_OK = 200,
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
};

struct LocationConfig
{
	std::string path = "/";
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
		void	handleRegularFile(const ServerConfig& serverConfig, std::string& path);
		void routing(const ServerConfig& serverConfig);
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
std::string& getExtension(const std::string& reqPath)
{
	size_t idx = reqPath.size() - 1;
	std::string tmp;
	while (idx >= 0)
	{
		if (reqPath[idx] == '.')
			break;
		tmp += reqPath[idx];
		idx--;
	}
	// TODO: should I make sure idx != 0? because we are sure it won't happen. 
	std::string res = std::string(tmp.begin(), tmp.end());
	return res;
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
		}
		else if (saveErr == EACCES)
		{
			_request.httpStatus = FORBIDDEN;
		}
		else
		{
			_request.httpStatus = INTERNAL_SERVER_ERR;
		}
		return false;
	}
	if (!S_ISREG(buf->st_mode) && !S_ISDIR(buf->st_mode))
	{
		_request.httpStatus = FORBIDDEN;
		return false;
	}
	return true;
}

void TestClient::handleDirectory(const ServerConfig& serverConfig, std::string& fullPath)
{
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	// if req method is GET
	if (toLower(_request.method) == "get")
	{
		if (lc.index.empty() && !lc.autoindex)
		{
			_request.httpStatus = FORBIDDEN;
			//_builder.buildErrorResponse(static_cast<int>(_request.httpStatus), serverConfig);
			std::cout << "location config has no index and no autoindex" << std::endl;
			return ;
		}
		else if (lc.index.empty()) // handleDirectoryListing
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
					if (ent->d_name == "." || ent->d_name == "..")
						continue;
					list.push_back(ent->d_name);
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
		else
		{
			std::string newPath = fullPath + lc.index;
			handleRegularFile(serverConfig, newPath);
		}
	}

	// if req method is POST
	if (toLower(_request.method) == "post")
	{

	}
	// if req method is DELETE
	if (toLower(_request.method) == "delete")
	{

	}
}

void	TestClient::handleRegularFile(const ServerConfig& serverConfig, std::string& path)
{
	std::string& extension = getExtension(path);
	if (extension == "py" || extension == "php")
	{
		return ; //handleCGI(serverConfig, extension);
	}
	else
	{
		return ; // handleStatic(serverConfig);
	}
}

void TestClient::routing(const ServerConfig& serverConfig)
{

	std::string root = serverConfig.root;
	const LocationConfig lc = serverConfig.locations[_locationIndex];
	if (lc.root.size() != 0)
		root = lc.root;
	std::string filesystemPath = root + _request.path;
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
			return ; //handleRedirect(301, _request.path + '/', serverConfig)// handle redirect and add / to the end;
		}
	}
	if (S_ISREG(buf.st_mode))
	{
		return handleRegularFile(serverConfig, _request.path);
	}

}

void initRequest(struct Request& req)
{
	req.method = "get";
	req.path = "/";
	req.httpStatus = REQ_OK;
}

void initLocationConfig(struct LocationConfig& lc, struct Request& request)
{
	lc.autoindex = false;
	lc.index = "";
	lc.methods.push_back("get");
	lc.methods.push_back("post");
	lc.path = "/";
	lc.redirectEnabled = false;
	lc.redirectStatus = 301;
	lc.redirectTarget = "www.abc";
	lc.root = "lcroot";
	lc.upload_store = "/upload";
}

void initServerConfig(struct ServerConfig& sc)
{
	sc.index = "";
	sc.root = "";
}

int main(void)
{
	struct ServerConfig	sc;
	initServerConfig(sc);
	std::vector<Request> requests;
	for (int i = 0; i < 1; i++)
	{
		struct Request req;
		initRequest(req);
		requests.push_back(req);
		struct LocationConfig lc;
		initLocationConfig(lc, requests[i]);
		sc.locations.push_back(lc);
	}
	
	size_t indexToTest = 1;
	TestClient client(indexToTest, requests[indexToTest]);
	client.routing(sc);
	
}