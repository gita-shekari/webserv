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
		if(it->path == path)
			return &(*it);
	}
	return NULL;
}
bool ResponseBuilder::isMethodAllowed(const std::string& method, const LocationConfig& location)
{
	std::vector<std::string>::const_iterator it;
	for (it = location.methods.begin(); it != location.methods.end(); ++it)
	{
		if (*it == method)
			return true;
	}
	return false;
}
std::string ResponseBuilder::resolveRoot(const ServerConfig& serverConfig, const LocationConfig& location)
{
	if (!location.root.empty())
		return location.root;
	return serverConfig.root;
}
std::string ResponseBuilder::resolveIndex(const ServerConfig& serverConfig, const LocationConfig& location)
{
	if (!location.index.empty())
		return location.index;
	return serverConfig.index;
}
Response ResponseBuilder::buildGetResponse(const Request& request, const ServerConfig& serverConfig, const LocationConfig& location)
{
	Response response;
	response.version = "HTTP/1.1";

	std::string root = resolveRoot(serverConfig, location);
	std::string index = resolveIndex(serverConfig, location);
	std::string filePath;
	if (request.path == "/")
		filePath = root + "/" + index;
	else
		filePath = root + request.path;
	std::string content;
	if (!getSource(filePath, content))
		return buildErrorResponse(404, serverConfig);
	response.statusCode = 200;
	response.reasonPhrase = "OK";
	response.body = content;
	response.headers["Content-Type"] = "text/html";
	response.headers["Content-Length"] = std::to_string(response.body.size());
	return response;
}
Response ResponseBuilder::buildPostResponse(const Request& request, const ServerConfig& serverConfig, const LocationConfig& location)
{
	// if(location.path = "/upload")
	std::string filePath = location.root + "/file"; // this path I need to process it
	std::ofstream file(filePath.c_str(), std::ios::out | std::ios::binary);
	if (!file.is_open())
		return buildErrorResponse(500, serverConfig);
	file.write(request.body.c_str(), request.body.size());
	if (!file.good())
		return buildErrorResponse(500, serverConfig);
	file.close();
	//creating response based on the extracted infos
	Response response;
	response.version = "HTTP/1.1";
	response.statusCode = 201;
	response.reasonPhrase = "Created";
	response.body = "File uploaded";
	response.headers["Content-Type"] = "text/plain";
	response.headers["Content-Length"] = std::to_string(response.body.size());
	return response;
}
std::string ResponseBuilder::getReasonPhrase(int statusCode)
{
	if (statusCode == 400)
		return "Bad Request";
	if (statusCode == 404)
		return "Not Found";
	if (statusCode == 405)
		return "Method Not Allowed";
	if (statusCode == 413)
		return "Payload Too Large";
	if (statusCode == 500)
		return "Internal Server Error";
	if (statusCode == 501)
		return "Not Implemented";
	if (statusCode == 505)
		return "HTTP Version Not Supported";

	return "Internal Server Error";
}

Response ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
	Response response;

	response.version = "HTTP/1.1";
	response.statusCode = statusCode;
	response.reasonPhrase = getReasonPhrase(statusCode);

	std::string errorPath =	serverConfig.root + "/" + serverConfig.error_page;
	if (!getSource(errorPath, response.body))
	{
		response.body =
			"<html><body><h1>"
			+ std::to_string(response.statusCode)
			+ " "
			+ response.reasonPhrase
			+ "</h1></body></html>";
	}
	response.headers["Content-Type"] = "text/html";
	response.headers["Content-Length"] = std::to_string(response.body.size());
	return response;
}
Response ResponseBuilder::buildResponse(const Request& request, const ServerConfig& serverConfig)
{

	const LocationConfig* location = findLocation(request.path, serverConfig);

	if (location == NULL)
		return buildErrorResponse(404, serverConfig);
	if (!isMethodAllowed(request.method, *location))
		return buildErrorResponse(405, serverConfig);
	if (request.method == "GET")
		return buildGetResponse(request, serverConfig, *location);

	if (request.method == "POST")
		return buildPostResponse(request, serverConfig, *location);

	// if (request.method == "DELETE")
	//     return buildDeleteResponse;
	return buildErrorResponse(501, serverConfig);
}

