#include "RoutingUtils.hpp"
#include <algorithm>

bool RoutingUtils::isMethodAllowed(const LocationConfig& lc, const std::string& method)
{
	std::vector<std::string>::const_iterator it = std::find(lc.methods.begin(), lc.methods.end(), method);
	if (it == lc.methods.end())
		return false;
	return true;
}
std::string RoutingUtils::joinPath(const std::string& root, const std::string& path)
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

std::string RoutingUtils::getCorrectFullPath(const ServerConfig& sc, const LocationConfig& lc, std::string reqPath)
{
	if (!lc.root.empty())
	{
		std::string relativePath = reqPath;
		if (relativePath.find(lc.path) == 0)
			relativePath = relativePath.substr(lc.path.size());

		return joinPath(lc.root, relativePath);
	}
	return joinPath(sc.root, reqPath);
}

bool RoutingUtils::isCGI(const LocationConfig& lc, const std::string& path)
{
	std::string ext;
	size_t dot = path.find_last_of('.');
	size_t slash = path.find_last_of('/');

	if (dot == std::string::npos)
		return false;
	if (slash != std::string::npos && slash > dot) //if dot belonging to a directory instead of the filename like cgi.file/test
		return false;
	ext = path.substr(dot);
	return (lc.cgi_extension == ext);
}
