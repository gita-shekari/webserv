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
Response build(const Request& request)
{
	Response response;
	if(request.method == "GET" && request.path == "\"")
	{

	}
	return response;
}

