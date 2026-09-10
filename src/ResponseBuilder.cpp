#include "ResponseBuilder.hpp"

/**
 * @brief This function converts a response to a valid foramt
 *
 * @param response
 * @return std::string
 */

std::string ResponseBuilder::serialize(const Response& response)
{
	std::string res;

	res += response.version;
	res += " ";
	res += std::to_string(response.statusCode);
	res += " ";
	res += response.reasonPhrase;
	res += "\r\n";
	std::map<std::string, std::string>::const_iterator it;
	for(it = response.headers.begin(); it != response.headers.end(); it++)
	{
		res += it->first;
		res += ": ";
		res += it->second;
		res += "\r\n";
	}
	res += "\r\n";
	res += response.body;
	return res;
}
// Request.method
//     ↓
// used to DECIDE what to do

// Request.path
//     ↓
// used to DECIDE which resource is requested

// Request.version
//     ↓
// may influence protocol handling

// Request.headers
//     ↓
// some may influence response behavior

// Request.body
//     ↓
// important especially for POST
bool ResponseBuilder::getSource(const std::string& path, std::string& content)
{
	std::ifstream src(path.c_str());
	if(!src.is_open())
		return false;
	std::stringstream buffer;
	buffer << src.rdbuf();
	content = buffer.str();
	return true;
}
const LocationConfig* ResponseBuilder::findLocation(const std::string& path, const ServerConfig& serverConfig)
{
	std::vector<LocationConfig>::const_iterator it;
	for(it = serverConfig.locations.begin(); it != serverConfig.locations.end(); it++)
	{
		std::cout << it->path << std::endl;
		std::cout << path << std::endl;
	}
	return NULL;
}
Response ResponseBuilder::buildGetResponse(const Request& request, const ServerConfig& serverConfig)
{
	Response response;
	response.version = "HTTP/1.1";

	const LocationConfig *location = findLocation(request.path, serverConfig);
	if (location == NULL)
	{
		response.statusCode = 404;
		response.reasonPhrase = "Not Found";
		response.body = "Not Found";
		response.headers["Content-Type"] = "text/plain";
		response.headers["Content-Length"] = std::to_string(response.body.size());
		return response;
	}
	// 1. Check GET is allowed
	bool getAllowed = false;
	std::vector<std::string>::const_iterator it;
	for (it = location->methods.begin();
		 it != location->methods.end();
		 ++it)
	{
		if (*it == "GET")
		{
			getAllowed = true;
			break;
		}
	}
	if (!getAllowed)
	{
		response.statusCode = 405;
		response.reasonPhrase = "Method Not Allowed";
		response.body = "Method Not Allowed";
		response.headers["Content-Type"] = "text/plain";
		response.headers["Content-Length"] =
			std::to_string(response.body.size());
		return response;
	}
	// 2. Resolve root
	std::string root;

	if (!location->root.empty())
		root = location->root;
	else
		root = serverConfig.root;
	// 3. Resolve index
	std::string index;
	if (!location->index.empty())
		index = location->index;
	else
		index = serverConfig.index;
	std::string filePath;
	if (request.path == "/")
		filePath = root + "/" + index;
	else
		filePath = root + request.path;
	// 4. Read file
	std::string content;
	if (!getSource(filePath, content))
	{
		response.statusCode = 404;
		response.reasonPhrase = "Not Found";
		response.body = "Not Found";
		response.headers["Content-Type"] = "text/plain";
		response.headers["Content-Length"] =
			std::to_string(response.body.size());
		return response;
	}
	// 5. Happy response
	response.statusCode = 200;
	response.reasonPhrase = "OK";
	response.body = content;
	response.headers["Content-Type"] = "text/html";
	response.headers["Content-Length"] =
		std::to_string(response.body.size());
	return response;
}

Response ResponseBuilder::build(const Request& request, const ServerConfig& serverConfig)
{
	Response response;
	if(request.method == "GET")
	{
		response = buildGetResponse(request, serverConfig);
	}
	// else if(request.method == "POST")
	// {

	// }
	// else if(request.method == "DELETE")
	// {

	// }
	else
	{
		response.statusCode = 405;
		response.reasonPhrase = "Method Not Allowed";
		response.body = "Method Not Allowed";
		response.headers["Content-Type"] = "text/plain";
		response.headers["Content-Length"] = std::to_string(response.body.size());
	}
	return response;
}

