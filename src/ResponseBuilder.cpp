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

std::string ResponseBuilder::getReasonPhrase(int statusCode)
{
	switch(statusCode)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";
		case 500: return "Internal Server Error";
		default: return "Unknown Status Code";
	}
}
Response ResponseBuilder::buildErrorResponse(int statusCode, const ServerConfig& serverConfig)
{
	Response response;
	response.statusCode = statusCode;
	response.reasonPhrase = getReasonPhrase(statusCode);
	response.version = "HTTP/1.1";
	response.headers["Content-Type"] = "text/html";
	std::map<int, std::string>::const_iterator it = serverConfig.error_pages.find(statusCode);
	if(it != serverConfig.error_pages.end())
	{
		std::string errorPagePath = serverConfig.root + "/" + it->second;
		if(getSource(errorPagePath, response.body))
		{
			response.headers["Content-Length"] = std::to_string(response.body.size());
			return response;
		}
	}
	else
	{
		response.statusCode = 500;
		response.reasonPhrase = getReasonPhrase(500);
		if(getSource(serverConfig.root + "/500.html", response.body))
		{
			response.headers["Content-Length"] = std::to_string(response.body.size());
			return response;
		}
		else
		{
			response.body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
			response.headers["Content-Length"] = std::to_string(response.body.size());
			return response;
		}
	}
}