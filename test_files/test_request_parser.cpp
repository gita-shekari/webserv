#include "RequestParser.hpp"
#include "TestRunner.hpp"

#include <cstddef>
#include <string>

static ParseStatus continueAfterLocation(
	RequestParser& parser,
	Request& request,
	const std::string& buffer,
	size_t maxBodySize
)
{
	ParseStatus status = parser.parseRequest(buffer, request);

	if (status != NEED_LOCATION)
		return status;
	request.effectiveMaxBodySize = maxBodySize;
	return parser.parseRequest(buffer, request);
}

static void testSimpleGet(TestRunner& tests)
{
	const std::string buffer =
		"get /docs/../index.html?name=codex%20user HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"X-Test:  first value  \r\n"
		"x-test: second value\r\n"
		"\r\n";
	RequestParser parser;
	Request request;

	ParseStatus status = parser.parseRequest(buffer, request);
	tests.checkEqual("request line asks for location", NEED_LOCATION, status);
	tests.checkEqual("method is normalized to uppercase", std::string("GET"), request.method);
	tests.checkEqual("path is decoded and normalized", std::string("/index.html"), request.path);
	tests.checkEqual("query remains encoded", std::string("name=codex%20user"), request.query);
	tests.checkEqual("HTTP version is stored", std::string("HTTP/1.1"), request.version);

	request.effectiveMaxBodySize = 1024;
	status = parser.parseRequest(buffer, request);
	tests.checkEqual("GET without body is complete", COMPLETE, status);
	tests.checkEqual("header name is lowercased", std::string("localhost"), request.headers["host"]);
	tests.checkEqual(
		"repeatable headers are combined",
		std::string("first value, second value"),
		request.headers["x-test"]
	);
	tests.checkEqual("GET body is empty", std::string(""), request.body);
	tests.checkEqual("all GET bytes are consumed", buffer.size(), parser.getConsumedBytes());
}

static void testIncompleteInput(TestRunner& tests)
{
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET / HTTP/1.1", request);
		tests.checkEqual("incomplete request line waits for CRLF", INCOMPLETE, status);
	}
	{
		RequestParser parser;
		Request request;
		const std::string buffer = "GET / HTTP/1.1\r\nHost: localhost\r\n";

		ParseStatus status = parser.parseRequest(buffer, request);
		tests.checkEqual("complete request line asks for location", NEED_LOCATION, status);
		request.effectiveMaxBodySize = 1024;
		status = parser.parseRequest(buffer, request);
		tests.checkEqual("incomplete headers wait for empty line", INCOMPLETE, status);
	}
}

static void testInvalidRequestLine(TestRunner& tests)
{
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET  HTTP/1.1\r\n", request);
		tests.checkEqual("missing request target is rejected", ERROR, status);
		tests.checkEqual("missing target produces 400", BAD_REQ, request.httpStatus);
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET / HTTP/1.0\r\n", request);
		tests.checkEqual("unsupported HTTP version is rejected", ERROR, status);
		tests.checkEqual("unsupported version produces 505", HTTP_VERSION_NOT_NSUP, request.httpStatus);
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET /bad%GG HTTP/1.1\r\n", request);
		tests.checkEqual("invalid request target is rejected", ERROR, status);
		tests.checkEqual("invalid target produces 400", BAD_REQ, request.httpStatus);
	}
}

static void testHeaderValidation(TestRunner& tests)
{
	struct HeaderCase
	{
		const char* name;
		const char* headers;
		HttpStatus expectedHttpStatus;
	};

	const HeaderCase cases[] = {
		{"missing Host is rejected", "User-Agent: test\r\n", BAD_REQ},
		{"empty Host is rejected", "Host:   \r\n", BAD_REQ},
		{"duplicate Host is rejected", "Host: one\r\nHost: two\r\n", BAD_REQ},
		{"header without colon is rejected", "Host: localhost\r\nBroken\r\n", BAD_REQ},
		{"space before colon is rejected", "Host : localhost\r\n", BAD_REQ}
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		RequestParser parser;
		Request request;
		std::string buffer = "GET / HTTP/1.1\r\n";
		buffer += cases[i].headers;
		buffer += "\r\n";

		ParseStatus status = continueAfterLocation(parser, request, buffer, 1024);
		tests.checkEqual(cases[i].name, ERROR, status);
		tests.checkEqual(
			std::string(cases[i].name) + " with expected status",
			cases[i].expectedHttpStatus,
			request.httpStatus
		);
	}
}

static void testContentLengthBody(TestRunner& tests)
{
	{
		const std::string buffer =
			"POST /upload HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length: 5\r\n"
			"\r\n"
			"helloEXTRA";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("complete Content-Length body succeeds", COMPLETE, status);
		tests.checkEqual("Content-Length controls body size", std::string("hello"), request.body);
		tests.checkEqual(
			"bytes after Content-Length are not consumed",
			buffer.size() - std::string("EXTRA").size(),
			parser.getConsumedBytes()
		);
	}
	{
		std::string buffer =
			"POST /upload HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length: 5\r\n"
			"\r\n"
			"hel";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("partial Content-Length body is incomplete", INCOMPLETE, status);
		buffer += "lo";
		status = parser.parseRequest(buffer, request);
		tests.checkEqual("Content-Length body completes after more bytes", COMPLETE, status);
		tests.checkEqual("completed partial body is correct", std::string("hello"), request.body);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Content-Length: nope\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("non-numeric Content-Length is rejected", ERROR, status);
		tests.checkEqual("invalid Content-Length produces 400", BAD_REQ, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Content-Length: 1\r\nContent-Length: 1\r\n\r\na";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("duplicate Content-Length is rejected", ERROR, status);
		tests.checkEqual("duplicate Content-Length produces 400", BAD_REQ, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Content-Length: 5\r\n\r\nhello";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 4);
		tests.checkEqual("Content-Length body over configured limit is rejected", ERROR, status);
		tests.checkEqual("oversized Content-Length body produces 413", PLAYLOAD_TOO_LARGE, request.httpStatus);
	}
}

static void testChunkedBody(TestRunner& tests)
{
	{
		const std::string buffer =
			"POST /upload HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nWiki\r\n5;name=value\r\npedia\r\n0\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("chunked body succeeds", COMPLETE, status);
		tests.checkEqual("chunks are combined", std::string("Wikipedia"), request.body);
		tests.checkEqual("all chunked bytes are consumed", buffer.size(), parser.getConsumedBytes());
	}
	{
		std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\n\r\n4\r\nWi";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("partial chunk data is incomplete", INCOMPLETE, status);
		buffer += "ki\r\n0\r\n\r\n";
		status = parser.parseRequest(buffer, request);
		tests.checkEqual("partial chunk completes after more bytes", COMPLETE, status);
		tests.checkEqual("completed chunk body is correct", std::string("Wiki"), request.body);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\n\r\nZ\r\ndata\r\n0\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("invalid chunk size is rejected", ERROR, status);
		tests.checkEqual("invalid chunk size produces 400", BAD_REQ, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 4);
		tests.checkEqual("chunked body over configured limit is rejected", ERROR, status);
		tests.checkEqual("oversized chunked body produces 413", PLAYLOAD_TOO_LARGE, request.httpStatus);
	}
}

static void testTransferEncodingValidation(TestRunner& tests)
{
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: gzip\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("unsupported Transfer-Encoding is rejected", ERROR, status);
		tests.checkEqual("unsupported Transfer-Encoding produces 501", NOT_IMPLEMENTED, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n0\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		tests.checkEqual("Transfer-Encoding with Content-Length is rejected", ERROR, status);
		tests.checkEqual("conflicting body framing produces 400", BAD_REQ, request.httpStatus);
	}
}

static void testReset(TestRunner& tests)
{
	RequestParser parser;
	Request request;
	const std::string first = "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n";
	const std::string second = "GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n";

	ParseStatus status = continueAfterLocation(parser, request, first, 100);
	tests.checkEqual("first request completes before reset", COMPLETE, status);

	parser.reset();
	request.clear();
	status = continueAfterLocation(parser, request, second, 100);
	tests.checkEqual("parser can be reused after reset", COMPLETE, status);
	tests.checkEqual("reset parser reads new path", std::string("/second"), request.path);
}

int main()
{
	TestRunner tests;

	testSimpleGet(tests);
	testIncompleteInput(tests);
	testInvalidRequestLine(tests);
	testHeaderValidation(tests);
	testContentLengthBody(tests);
	testChunkedBody(tests);
	testTransferEncodingValidation(tests);
	testReset(tests);

	return tests.finish();
}
