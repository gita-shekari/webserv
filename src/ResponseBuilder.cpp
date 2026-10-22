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
std::string ResponseBuilder::getContentType(const std::string& path)
{
	size_t dotPos = path.find_last_of('.');
	if(dotPos == std::string::npos)
		return "application/octet-stream";
	std::string extension = path.substr(dotPos + 1);
	if(extension == "html" || extension == "htm")
		return "text/html";
	else if(extension == "css")
		return "text/css";
	else if(extension == "js")
		return "application/javascript";
	else if(extension == "json")
		return "application/json";
	else if(extension == "png")
		return "image/png";
	else if(extension == "jpg" || extension == "jpeg")
		return "image/jpeg";
	else if(extension == "gif")
		return "image/gif";
	else if(extension == "txt")
		return "text/plain";
	else
		return "application/octet-stream";
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

Response ResponseBuilder::buildStaticResponse(const std::string& filePath, const ServerConfig& serverConfig)
{
	Response response;
	if (!getSource(filePath, response.body))
		return buildErrorResponse(404, serverConfig);
	response.version = "HTTP/1.1";
	response.statusCode = 200;
	response.reasonPhrase = getReasonPhrase(200);
	response.headers["Content-Type"] = getContentType(filePath);
	response.headers["Content-Length"] = std::to_string(response.body.size());
	return response;
}

Response ResponseBuilder::buildRedirectResponse(int statusCode, const std::string& location)
{
	Response response;
	response.version = "HTTP/1.1";
	response.statusCode = statusCode;
	response.reasonPhrase = getReasonPhrase(statusCode);
	response.headers["Location"] = location;
	//we don't need a body for redirect responses, so we can leave it empty	or we can set it to a default message	
	response.body.clear();
	response.headers["Content-Length"] = "0";
	return response;
}
Response ResponseBuilder::buildNoContentResponse()
{
	Response response;
	response.version = "HTTP/1.1";
	response.statusCode = 204;
	response.reasonPhrase = getReasonPhrase(204);
	response.body.clear();
	response.headers["Content-Length"] = "0";
	return response;
}
Response ResponseBuilder::buildCreatedResponse(const std::string& location)
{
	Response response;

	response.version = "HTTP/1.1";
	response.statusCode = 201;
	response.reasonPhrase = getReasonPhrase(201);
	response.body.clear();

	response.headers["Location"] = location;
	response.headers["Content-Length"] = "0";

	return response;
}

Response ResponseBuilder::buildListingResponse(const std::vector<std::string>& list, const std::string& requestPath)
{
	Response response;

	response.version = "HTTP/1.1";
	response.statusCode = 200;
	response.reasonPhrase = getReasonPhrase(200);

	std::string body;

	body += "<!DOCTYPE html>\n";
	body += "<html>\n";
	body += "<head>\n";
	body += "<title>Index of ";
	body += requestPath;
	body += "</title>\n";
	body += "</head>\n";
	body += "<body>\n";
	body += "<h1>Index of ";
	body += requestPath;
	body += "</h1>\n";
	body += "<ul>\n";

	for (size_t i = 0; i < list.size(); ++i)
	{
		body += "<li><a href=\"";

		if (!requestPath.empty() && requestPath[requestPath.size() - 1] != '/')
		{
			body += requestPath;
			body += "/";
		}
		else
			body += requestPath;

		body += list[i];
		body += "\">";
		body += list[i];
		body += "</a></li>\n";
	}

	body += "</ul>\n";
	body += "</body>\n";
	body += "</html>\n";

	response.body = body;

	response.headers["Content-Type"] = "text/html";
	response.headers["Content-Length"] = std::to_string(response.body.size());

	return response;
}