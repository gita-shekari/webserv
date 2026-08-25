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

Response ResponseBuilder::buildGetResponse(const Request& request)
{
	Response response;
	response.version = "HTTP/1.1";
	std::string content;
	if(request.path == "/")
	{
		if(getSource("www/index.html", content))
		{
			response.statusCode = 200;
			response.reasonPhrase = "OK";
			response.body = content;
			response.headers["Content-Type"] = "text/html";
			response.headers["Content-Length"] = std::to_string(response.body.size());
		}
		else
		{
			response.statusCode = 404;
			response.reasonPhrase = "Not Found";
			response.body = "Not Found";
			response.headers["Content-Type"] = "text/plain";
			response.headers["Content-Length"] = std::to_string(response.body.size());
		}
	}
	return response;
}
Response ResponseBuilder::build(const Request& request)
{
	Response response;
	if(request.method == "GET")
	{
		response = buildGetResponse(request);
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

