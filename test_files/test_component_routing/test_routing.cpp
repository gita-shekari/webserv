#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <dirent.h>
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

enum HttpStatus
{
	REQ_OK = 200,
	MOVED_PERMANENTLY = 301,
	TEMPORARY_REDIRECT = 307,
	FORBIDDEN = 403,
	PAGE_NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	INTERNAL_SERVER_ERROR = 500
};

enum RouteAction
{
	ACTION_NONE,
	ACTION_CONFIG_REDIRECT,
	ACTION_DIRECTORY_REDIRECT,
	ACTION_STATIC,
	ACTION_CGI,
	ACTION_DIRECTORY_LISTING,
	ACTION_UPLOAD,
	ACTION_DELETE,
	ACTION_ERROR
};

struct Request
{
	std::string method;
	std::string path;

	Request(const std::string& requestMethod, const std::string& requestPath)
		: method(requestMethod), path(requestPath) {}
};

struct LocationConfig
{
	std::string path;
	std::vector<std::string> methods;
	std::string root;
	std::string index;
	bool autoindex;
	std::string uploadStore;
	std::map<std::string, std::string> cgiHandlers;
	bool redirectEnabled;
	int redirectStatus;
	std::string redirectTarget;

	LocationConfig()
		: autoindex(false), redirectEnabled(false), redirectStatus(0) {}
};

struct ServerConfig
{
	std::string root;
};

struct RouteResult
{
	RouteAction action;
	int status;
	std::string target;
	std::vector<std::string> entries;

	RouteResult()
		: action(ACTION_NONE), status(REQ_OK) {}
};

static std::string joinPath(const std::string& root, const std::string& path)
{
	if (root.empty())
		return path;
	if (path.empty())
		return root;

	const bool rootEndsSlash = root[root.size() - 1] == '/';
	const bool pathStartsSlash = path[0] == '/';

	if (rootEndsSlash && pathStartsSlash)
		return root + path.substr(1);
	if (!rootEndsSlash && !pathStartsSlash)
		return root + "/" + path;
	return root + path;
}

static bool endsWithSlash(const std::string& path)
{
	return !path.empty() && path[path.size() - 1] == '/';
}

static bool isMethodAllowed(const LocationConfig& location,
							const std::string& method)
{
	return std::find(location.methods.begin(), location.methods.end(), method)
		!= location.methods.end();
}

static bool isCgi(const LocationConfig& location, const std::string& path)
{
	const size_t dot = path.find_last_of('.');
	const size_t slash = path.find_last_of('/');

	if (dot == std::string::npos)
		return false;
	if (slash != std::string::npos && slash > dot)
		return false;

	return location.cgiHandlers.find(path.substr(dot))
		!= location.cgiHandlers.end();
}

static int inspectPath(const std::string& path, struct stat& info)
{
	if (stat(path.c_str(), &info) == 0)
		return REQ_OK;
	if (errno == ENOENT || errno == ENOTDIR)
		return PAGE_NOT_FOUND;
	if (errno == EACCES || errno == ELOOP)
		return FORBIDDEN;
	return INTERNAL_SERVER_ERROR;
}

static bool listDirectory(const std::string& path,
						  std::vector<std::string>& entries)
{
	DIR* directory = opendir(path.c_str());
	if (directory == NULL)
		return false;

	while (true)
	{
		errno = 0;
		struct dirent* entry = readdir(directory);
		if (entry == NULL)
		{
			const int readError = errno;
			closedir(directory);
			return readError == 0;
		}

		const std::string name(entry->d_name);
		if (!name.empty() && name[0] == '.')
			continue;
		entries.push_back(name);
	}
}

class TestRouter
{
public:
	RouteResult route(const ServerConfig& server,
					  const LocationConfig& location,
					  const Request& request) const
	{
		RouteResult result;

		if (location.redirectEnabled)
		{
			result.action = ACTION_CONFIG_REDIRECT;
			result.status = location.redirectStatus;
			result.target = location.redirectTarget;
			return result;
		}

		if (!isMethodAllowed(location, request.method))
			return errorResult(METHOD_NOT_ALLOWED);

		const std::string root = location.root.empty()
			? server.root : location.root;
		const std::string fullPath = joinPath(root, request.path);

		if (request.method == "GET")
			return routeGet(location, request, fullPath);
		if (request.method == "POST")
			return routePost(location, request, fullPath);
		if (request.method == "DELETE")
			return routeDelete(fullPath);

		return errorResult(METHOD_NOT_ALLOWED);
	}

private:
	static RouteResult errorResult(int status)
	{
		RouteResult result;
		result.action = ACTION_ERROR;
		result.status = status;
		return result;
	}

	static RouteResult actionResult(RouteAction action,
								int status,
								const std::string& target)
	{
		RouteResult result;
		result.action = action;
		result.status = status;
		result.target = target;
		return result;
	}

	RouteResult routeGet(const LocationConfig& location,
						 const Request& request,
						 const std::string& fullPath) const
	{
		struct stat info;
		const int status = inspectPath(fullPath, info);
		if (status != REQ_OK)
			return errorResult(status);

		if (S_ISREG(info.st_mode))
		{
			if (isCgi(location, fullPath))
				return actionResult(ACTION_CGI, REQ_OK, fullPath);
			return actionResult(ACTION_STATIC, REQ_OK, fullPath);
		}

		if (!S_ISDIR(info.st_mode))
			return errorResult(FORBIDDEN);

		if (!endsWithSlash(request.path))
			return actionResult(ACTION_DIRECTORY_REDIRECT,
				MOVED_PERMANENTLY, request.path + "/");

		if (!location.index.empty())
		{
			const std::string indexPath = joinPath(fullPath, location.index);
			if (isCgi(location, indexPath))
				return actionResult(ACTION_CGI, REQ_OK, indexPath);
			return actionResult(ACTION_STATIC, REQ_OK, indexPath);
		}

		if (!location.autoindex)
			return errorResult(FORBIDDEN);

		RouteResult result = actionResult(
			ACTION_DIRECTORY_LISTING, REQ_OK, fullPath);
		if (!listDirectory(fullPath, result.entries))
			return errorResult(INTERNAL_SERVER_ERROR);
		std::sort(result.entries.begin(), result.entries.end());
		return result;
	}

	RouteResult routePost(const LocationConfig& location,
						  const Request& request,
						  const std::string& fullPath) const
	{
		struct stat info;
		const int status = inspectPath(fullPath, info);

		if (status == REQ_OK && S_ISDIR(info.st_mode))
		{
			if (!endsWithSlash(request.path))
				return actionResult(ACTION_DIRECTORY_REDIRECT,
					TEMPORARY_REDIRECT, request.path + "/");

			if (!location.index.empty())
			{
				const std::string indexPath = joinPath(fullPath, location.index);
				if (isCgi(location, indexPath))
					return actionResult(ACTION_CGI, REQ_OK, indexPath);
			}
		}
		else if (status == REQ_OK && S_ISREG(info.st_mode))
		{
			if (isCgi(location, fullPath))
				return actionResult(ACTION_CGI, REQ_OK, fullPath);
		}
		else if (status == REQ_OK)
		{
			return errorResult(FORBIDDEN);
		}

		if (!location.uploadStore.empty())
			return actionResult(ACTION_UPLOAD, REQ_OK, location.uploadStore);

		if (status != REQ_OK)
			return errorResult(status);
		return errorResult(FORBIDDEN);
	}

	RouteResult routeDelete(const std::string& fullPath) const
	{
		struct stat info;
		const int status = inspectPath(fullPath, info);
		if (status != REQ_OK)
			return errorResult(status);
		if (!S_ISREG(info.st_mode))
			return errorResult(FORBIDDEN);
		return actionResult(ACTION_DELETE, REQ_OK, fullPath);
	}
};

static std::string findFixtureRoot()
{
	const char* candidates[] = {
		"test_files/test_routing/fixtures/www",
		"fixtures/www"
	};

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
	{
		struct stat info;
		if (stat(candidates[i], &info) == 0 && S_ISDIR(info.st_mode))
			return candidates[i];
	}
	return "";
}

static LocationConfig baseLocation()
{
	LocationConfig location;
	location.path = "/";
	location.methods.push_back("GET");
	location.methods.push_back("POST");
	location.methods.push_back("DELETE");
	return location;
}

static bool contains(const std::vector<std::string>& entries,
					 const std::string& expected)
{
	return std::find(entries.begin(), entries.end(), expected) != entries.end();
}

class TestSuite
{
public:
	TestSuite() : _passed(0), _failed(0) {}

	void check(const std::string& name,
			   const RouteResult& actual,
			   RouteAction expectedAction,
			   int expectedStatus,
			   const std::string& expectedTarget = "")
	{
		const bool passed = actual.action == expectedAction
			&& actual.status == expectedStatus
			&& (expectedTarget.empty() || actual.target == expectedTarget);

		if (passed)
		{
			++_passed;
			std::cout << "[PASS] " << name << "\n";
		}
		else
		{
			++_failed;
			std::cout << "[FAIL] " << name
				<< " action=" << actual.action
				<< " status=" << actual.status
				<< " target=" << actual.target << "\n";
		}
	}

	void checkCondition(const std::string& name, bool condition)
	{
		if (condition)
		{
			++_passed;
			std::cout << "[PASS] " << name << "\n";
		}
		else
		{
			++_failed;
			std::cout << "[FAIL] " << name << "\n";
		}
	}

	int finish() const
	{
		std::cout << "\n" << _passed << " passed, "
			<< _failed << " failed\n";
		return _failed == 0 ? 0 : 1;
	}

private:
	int _passed;
	int _failed;
};

int main()
{
	const std::string fixtureRoot = findFixtureRoot();
	if (fixtureRoot.empty())
	{
		std::cerr << "Cannot find test_files/test_routing/fixtures/www\n";
		return 2;
	}

	ServerConfig server;
	server.root = fixtureRoot;
	TestRouter router;
	TestSuite tests;

	LocationConfig listing = baseLocation();
	listing.autoindex = true;
	RouteResult result = router.route(server, listing, Request("GET", "/listing/"));
	tests.check("GET directory listing", result,
		ACTION_DIRECTORY_LISTING, REQ_OK,
		joinPath(fixtureRoot, "/listing/"));
	tests.checkCondition("listing contains alpha.txt",
		contains(result.entries, "alpha.txt"));
	tests.checkCondition("listing contains nested directory",
		contains(result.entries, "nested"));
	tests.checkCondition("listing contains spaced filename",
		contains(result.entries, "space name.txt"));
	tests.checkCondition("listing contains HTML-sensitive filename",
		contains(result.entries, "<tag>&.txt"));
	tests.checkCondition("listing hides .hidden.txt",
		!contains(result.entries, ".hidden.txt"));

	tests.check("GET directory without slash", router.route(
		server, listing, Request("GET", "/listing")),
		ACTION_DIRECTORY_REDIRECT, MOVED_PERMANENTLY, "/listing/");

	LocationConfig htmlIndex = baseLocation();
	htmlIndex.index = "index.html";
	tests.check("GET static directory index", router.route(
		server, htmlIndex, Request("GET", "/with_index/")),
		ACTION_STATIC, REQ_OK,
		joinPath(fixtureRoot, "/with_index/index.html"));

	LocationConfig cgi = baseLocation();
	cgi.cgiHandlers[".py"] = "/usr/bin/python3";
	cgi.index = "index.py";
	tests.check("GET CGI directory index", router.route(
		server, cgi, Request("GET", "/cgi_index/")),
		ACTION_CGI, REQ_OK,
		joinPath(fixtureRoot, "/cgi_index/index.py"));
	tests.check("GET direct CGI file", router.route(
		server, cgi, Request("GET", "/cgi/direct.py")),
		ACTION_CGI, REQ_OK,
		joinPath(fixtureRoot, "/cgi/direct.py"));
	tests.check("GET regular file in CGI location", router.route(
		server, cgi, Request("GET", "/cgi/plain.txt")),
		ACTION_STATIC, REQ_OK,
		joinPath(fixtureRoot, "/cgi/plain.txt"));

	LocationConfig noAutoindex = baseLocation();
	tests.check("GET directory without index or autoindex", router.route(
		server, noAutoindex, Request("GET", "/no_autoindex/")),
		ACTION_ERROR, FORBIDDEN);
	tests.check("GET missing path", router.route(
		server, listing, Request("GET", "/missing")),
		ACTION_ERROR, PAGE_NOT_FOUND);

	tests.check("POST direct CGI", router.route(
		server, cgi, Request("POST", "/cgi/direct.py")),
		ACTION_CGI, REQ_OK,
		joinPath(fixtureRoot, "/cgi/direct.py"));
	tests.check("POST CGI directory index", router.route(
		server, cgi, Request("POST", "/cgi_index/")),
		ACTION_CGI, REQ_OK,
		joinPath(fixtureRoot, "/cgi_index/index.py"));

	LocationConfig upload = baseLocation();
	upload.autoindex = true;
	upload.uploadStore = "test_files/test_routing/fixtures/uploads";
	tests.check("POST directory without slash", router.route(
		server, upload, Request("POST", "/listing")),
		ACTION_DIRECTORY_REDIRECT, TEMPORARY_REDIRECT, "/listing/");
	tests.check("POST upload to directory endpoint", router.route(
		server, upload, Request("POST", "/listing/")),
		ACTION_UPLOAD, REQ_OK, upload.uploadStore);
	tests.check("POST upload creates missing resource", router.route(
		server, upload, Request("POST", "/new-resource.txt")),
		ACTION_UPLOAD, REQ_OK, upload.uploadStore);

	tests.check("DELETE regular file", router.route(
		server, listing, Request("DELETE", "/delete/remove-me.txt")),
		ACTION_DELETE, REQ_OK,
		joinPath(fixtureRoot, "/delete/remove-me.txt"));
	tests.check("DELETE directory forbidden", router.route(
		server, listing, Request("DELETE", "/listing/")),
		ACTION_ERROR, FORBIDDEN);
	tests.check("DELETE missing path", router.route(
		server, listing, Request("DELETE", "/missing.txt")),
		ACTION_ERROR, PAGE_NOT_FOUND);

	LocationConfig getOnly = baseLocation();
	getOnly.methods.clear();
	getOnly.methods.push_back("GET");
	tests.check("disallowed method", router.route(
		server, getOnly, Request("POST", "/listing/")),
		ACTION_ERROR, METHOD_NOT_ALLOWED);

	LocationConfig redirect = baseLocation();
	redirect.redirectEnabled = true;
	redirect.redirectStatus = MOVED_PERMANENTLY;
	redirect.redirectTarget = "/new-place";
	tests.check("configured redirect has priority", router.route(
		server, redirect, Request("DELETE", "/missing")),
		ACTION_CONFIG_REDIRECT, MOVED_PERMANENTLY, "/new-place");

	tests.check("CGI-looking directory remains directory", router.route(
		server, cgi, Request("POST", "/folder.py")),
		ACTION_DIRECTORY_REDIRECT, TEMPORARY_REDIRECT, "/folder.py/");

	return tests.finish();
}
