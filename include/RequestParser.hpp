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
	BODY,
	DONE
};

enum	BodyType
{
	NO_BODY,
	CHUNKED_BODY,
	CONTENT_LENGTH_BODY
};

enum	ChunkState
{
	CHUNK_SIZE,
	CHUNK_DATA
};

class RequestParser
{
	public:
		RequestParser();
		ParseStatus	parse(const std::string& buffer, Request& req);
		size_t	getConsumedBytes() const;
		void	reset();

		//debug
		void	printAttributes() const;

	private:
		ParseSection	_section;
		size_t			_cursor;
		size_t			_contentLength;
		BodyType		_body;
		ChunkState		_chunkState;
		size_t			_chunkSize;

		ParseStatus	parseRequestLine(const std::string& buffer, Request& req);
		ParseStatus	parseHeaders(const std::string& buffer, Request& req);
		ParseStatus	parseBody(const std::string& buffer, Request& req);

		ParseStatus	judgeBody(const Request& req);

		ParseStatus	parseContentLengthBody(const std::string& buffer, Request& req);
		ParseStatus	parseChunkedBody(const std::string& buffer, Request& req);

		//parse size
		bool	parseDecSize(const std::string& str, size_t& size);
		bool	parseHexSize(const std::string& str, size_t& size);
	
		//utils
		std::string	toLower(const std::string& str);
		std::string	trim(const std::string& str);
};