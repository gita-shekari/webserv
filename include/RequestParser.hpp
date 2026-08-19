#pragma once

# include "Request.hpp"

enum	ParseStatus
{
	INCOMPLETE,
	COMPLETE,
	ERROR
};

enum	ParseSection
{
	REQUEST_LINE,
	HEADERS,
	BODY
};

class RequestParser
{
	public:
		RequestParser();
		ParseStatus	parse(const std::string& buffer, Request& req);

	private:
		ParseSection	_section;
		size_t			_cursor;
		size_t			_contentLength;

		ParseStatus	parseRequestLine(const std::string& buffer, Request& req);

};
