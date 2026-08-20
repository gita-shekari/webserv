#pragma once

#include "Request.hpp"
#include "Response.hpp"
#include <string>
#include <iostream>
#include <map>

class ResponseBuilder
{
public:
	Response build(const Request& request);
	std::string serialize(const Response& response);
};
