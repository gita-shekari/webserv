#include "RequestParser.hpp"
#include "TestRunner.hpp"

#include <cstddef>
#include <string>

// Print meaningful parser states instead of enum numbers in failures.
static std::string statusName(ParseStatus status)
{
    switch (status)
    {
        case INCOMPLETE: return "INCOMPLETE";
        case NEED_LOCATION: return "NEED_LOCATION";
        case COMPLETE: return "COMPLETE";
        case ERROR: return "ERROR";
    }
    return "UNKNOWN(" + std::to_string(static_cast<int>(status)) + ")";
}

static void checkStatus(TestRunner& tests, const std::string& name,
    ParseStatus expected, ParseStatus actual)
{
    tests.checkEqual(name, statusName(expected), statusName(actual));
}

// This is a log summary, not an additional assertion.
static void reportGroup(const std::string& name, int failedAssertions)
{
    if (failedAssertions == 0)
        std::cout << "[PASS] " << name << '\n';
    else
        std::cerr << "[FAIL] " << name << ": "
            << failedAssertions << " assertion(s) failed\n";
}

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
		"GET /docs/../index.html?name=codex%20user HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"X-Test:  first value  \r\n"
		"x-test: second value\r\n"
		"\r\n";
	RequestParser parser;
	Request request;

	ParseStatus status = parser.parseRequest(buffer, request);
	checkStatus(tests, "pauses after the request line to obtain location rules", NEED_LOCATION, status);
	tests.checkEqual("preserves the GET method", std::string("GET"), request.method);
	tests.checkEqual("normalizes dot segments in the request path", std::string("/index.html"), request.path);
	tests.checkEqual("query remains encoded", std::string("name=codex%20user"), request.query);
	tests.checkEqual("preserves the HTTP version", std::string("HTTP/1.1"), request.version);

	request.effectiveMaxBodySize = 1024;
	status = parser.parseRequest(buffer, request);
	checkStatus(tests, "accepts GET without a body", COMPLETE, status);
	tests.checkEqual("recognizes header names regardless of case", std::string("localhost"), request.headers["host"]);
	tests.checkEqual(
		"repeatable headers are combined",
		std::string("first value, second value"),
		request.headers["x-test"]
	);
	tests.checkEqual("keeps body empty for a bodyless GET", std::string(""), request.body);
	tests.checkEqual("consumes exactly the complete GET request", buffer.size(), parser.getConsumedBytes());
}

static void testIncompleteInput(TestRunner& tests)
{
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET / HTTP/1.1", request);
		checkStatus(tests, "waits for a complete request line", INCOMPLETE, status);
	}
	{
		RequestParser parser;
		Request request;
		std::string buffer = "GET / HTTP/1.1\r\nHost: localhost\r\n";

		ParseStatus status = continueAfterLocation(parser, request, buffer, 1024);
		checkStatus(tests, "waits for complete headers", INCOMPLETE, status);
		buffer += "\r\n";
		status = parser.parseRequest(buffer, request);
		checkStatus(tests, "accepts headers completed by a later fragment", COMPLETE, status);
	}
}

static void testInvalidRequestLine(TestRunner& tests)
{
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET  HTTP/1.1\r\n", request);
		checkStatus(tests, "missing request target is rejected", ERROR, status);
		tests.checkEqual("missing target produces 400", BAD_REQ, request.httpStatus);
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET / HTTP/1.0\r\n", request);
		checkStatus(tests, "unsupported HTTP version is rejected", ERROR, status);
		tests.checkEqual("unsupported version produces 505", HTTP_VERSION_NOT_NSUP, request.httpStatus);
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = parser.parseRequest("GET /bad%GG HTTP/1.1\r\n", request);
		checkStatus(tests, "invalid request target is rejected", ERROR, status);
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
		checkStatus(tests, cases[i].name, ERROR, status);
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
		checkStatus(tests, "accepts a complete Content-Length body", COMPLETE, status);
		tests.checkEqual("uses Content-Length to delimit the body", std::string("hello"), request.body);
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
		checkStatus(tests, "waits for the full Content-Length body", INCOMPLETE, status);
		buffer += "lo";
		status = parser.parseRequest(buffer, request);
		checkStatus(tests, "accepts a Content-Length body delivered in fragments", COMPLETE, status);
		tests.checkEqual("preserves body content across fragments", std::string("hello"), request.body);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Content-Length: nope\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		checkStatus(tests, "non-numeric Content-Length is rejected", ERROR, status);
		tests.checkEqual("invalid Content-Length produces 400", BAD_REQ, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Content-Length: 1\r\nContent-Length: 1\r\n\r\na";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		checkStatus(tests, "duplicate Content-Length is rejected", ERROR, status);
		tests.checkEqual("duplicate Content-Length produces 400", BAD_REQ, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Content-Length: 5\r\n\r\nhello";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 4);
		checkStatus(tests, "Content-Length body over configured limit is rejected", ERROR, status);
		tests.checkEqual("oversized Content-Length body produces 413", PAYLOAD_TOO_LARGE, request.httpStatus);
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
		checkStatus(tests, "accepts a complete chunked body", COMPLETE, status);
		tests.checkEqual("combines decoded chunks into the body", std::string("Wikipedia"), request.body);
		tests.checkEqual("all chunked bytes are consumed", buffer.size(), parser.getConsumedBytes());
	}
	{
		std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\n\r\n4\r\nWi";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		checkStatus(tests, "waits for complete chunk data", INCOMPLETE, status);
		buffer += "ki\r\n0\r\n\r\n";
		status = parser.parseRequest(buffer, request);
		checkStatus(tests, "accepts a chunk delivered in fragments", COMPLETE, status);
		tests.checkEqual("preserves chunk content across fragments", std::string("Wiki"), request.body);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\n\r\nZ\r\ndata\r\n0\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		checkStatus(tests, "invalid chunk size is rejected", ERROR, status);
		tests.checkEqual("invalid chunk size produces 400", BAD_REQ, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 4);
		checkStatus(tests, "chunked body over configured limit is rejected", ERROR, status);
		tests.checkEqual("oversized chunked body produces 413", PAYLOAD_TOO_LARGE, request.httpStatus);
	}
}

static void testTransferEncodingValidation(TestRunner& tests)
{
    {
        RequestParser parser;
        Request request;
        const std::string buffer =
            "POST / HTTP/1.1\r\nHost: localhost\r\n"
            "Transfer-Encoding: chunked\r\n"
            "tRaNsFeR-EnCoDiNg: chunked\r\n\r\n0\r\n\r\n";
        ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
        checkStatus(tests, "rejects duplicate Transfer-Encoding regardless of case", ERROR, status);
        tests.checkEqual("duplicate Transfer-Encoding produces 400",
            BAD_REQ, request.httpStatus);
    }

	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: gzip\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		checkStatus(tests, "unsupported Transfer-Encoding is rejected", ERROR, status);
		tests.checkEqual("unsupported Transfer-Encoding produces 501", NOT_IMPLEMENTED, request.httpStatus);
	}
	{
		const std::string buffer =
			"POST / HTTP/1.1\r\nHost: localhost\r\n"
			"Transfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n0\r\n\r\n";
		RequestParser parser;
		Request request;

		ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
		checkStatus(tests, "Transfer-Encoding with Content-Length is rejected", ERROR, status);
		tests.checkEqual("conflicting body framing produces 400", BAD_REQ, request.httpStatus);
	}
}

static void testPercentEncoding(TestRunner& tests)
{
    struct PathCase { const char* target; const char* decoded; };
    const PathCase valid[] = {
        {"/hello%20world", "/hello world"},
        {"/%41%42%43", "/ABC"},
        {"/%7euser", "/~user"},
        {"/%7Euser", "/~user"}
    };
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i)
    {
        RequestParser parser;
        Request request;
        const std::string target(valid[i].target);
        const std::string buffer = "GET " + target
            + "?name=a%20b HTTP/1.1\r\nHost: localhost\r\n\r\n";
        ParseStatus status = continueAfterLocation(parser, request, buffer, 100);
        checkStatus(tests, "accepts percent-encoded path " + target, COMPLETE, status);
        tests.checkEqual("decodes path " + target,
            std::string(valid[i].decoded), request.path);
        tests.checkEqual("preserves query encoding for " + target,
            std::string("name=a%20b"), request.query);
    }
    const char* invalid[] = {"/hello%", "/hello%2", "/hello%GG"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        RequestParser parser;
        Request request;
        const std::string target(invalid[i]);
        ParseStatus status = continueAfterLocation(parser, request,
            "GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n", 100);
        checkStatus(tests, "rejects malformed path encoding " + target, ERROR, status);
        tests.checkEqual("malformed path encoding produces 400: " + target,
            BAD_REQ, request.httpStatus);
    }
}

static void testReset(TestRunner& tests)
{
	RequestParser parser;
	Request request;
	const std::string first = "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n";
	const std::string second = "GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n";

	ParseStatus status = continueAfterLocation(parser, request, first, 100);
	checkStatus(tests, "first request completes before reset", COMPLETE, status);

	parser.reset();
	request.clear();
	status = continueAfterLocation(parser, request, second, 100);
	checkStatus(tests, "parser can be reused after reset", COMPLETE, status);
	tests.checkEqual("reset parser reads new path", std::string("/second"), request.path);
}


static std::string caseName(const std::string& name, size_t position)
{
	return name + " at byte " + std::to_string(position);
}

// Every split gets a new parser; both calls use the same growing buffer.
static void checkEverySplit(
	TestRunner& tests,
	const std::string& name,
	const std::string& complete,
	const std::string& expectedBody,
	size_t limit
)
{
	const int failuresBefore = tests.failedCount();
	size_t failedCuts = 0;
	tests.setPrintPass(false);
	for (size_t cut = 0; cut < complete.size(); ++cut)
	{
		const int failuresAtCut = tests.failedCount();
		RequestParser parser;
		Request request;
		std::string buffer = complete.substr(0, cut);
		ParseStatus status =
			continueAfterLocation(parser, request, buffer, limit);
		const std::string label = caseName(name, cut);

		checkStatus(tests, label + ": prefix waits", INCOMPLETE, status);
		buffer.append(complete, cut, std::string::npos);
		status = continueAfterLocation(parser, request, buffer, limit);

		checkStatus(tests, label + ": completes", COMPLETE, status);
		tests.checkEqual(label + ": body", expectedBody, request.body);
		tests.checkEqual(label + ": method", std::string("POST"), request.method);
		tests.checkEqual(label + ": path", std::string("/upload"), request.path);
		tests.checkEqual(label + ": Host", std::string("localhost"),
			request.headers["host"]);
		tests.checkEqual(label + ": consumed", complete.size(),
			parser.getConsumedBytes());
		if (tests.failedCount() != failuresAtCut)
			++failedCuts;
	}
	tests.setPrintPass(true);
	if (failedCuts == 0)
		reportGroup(name + " request parses correctly at every split position ("
			+ std::to_string(complete.size()) + " positions)", 0);
	else
		std::cerr << "[FAIL] " << name << " split checks: " << failedCuts
			<< " split position(s) failed; "
			<< tests.failedCount() - failuresBefore << " assertion(s) failed\n";
}

static void testEverySplit(TestRunner& tests)
{
	const std::string headers =
		"POST /upload HTTP/1.1\r\nHost: localhost\r\n";
	checkEverySplit(tests, "Content-Length",
		headers + "Content-Length: 5\r\n\r\nhello", "hello", 5);
	checkEverySplit(tests, "Chunked",
		headers + "Transfer-Encoding: chunked\r\n\r\n"
		"2\r\nhe\r\n3\r\nllo\r\n0\r\n\r\n", "hello", 5);
}

static void testChunkTerminatorSplit(TestRunner& tests)
{
	RequestParser parser;
	Request request;
	std::string buffer =
		"POST /upload HTTP/1.1\r\nHost: localhost\r\n"
		"Transfer-Encoding: chunked\r\n\r\n"
		"4\r\nWiki\r\n0\r\n";

	ParseStatus status = continueAfterLocation(parser, request, buffer, 4);
	checkStatus(tests, "waits for the complete chunked terminator", INCOMPLETE, status);
	tests.checkEqual("body is retained while waiting", std::string("Wiki"),
		request.body);

	buffer += "\r\n";
	status = parser.parseRequest(buffer, request);
	checkStatus(tests, "accepts a chunked terminator delivered in fragments", COMPLETE, status);
	tests.checkEqual("preserves body when resuming the chunked terminator", std::string("Wiki"),
		request.body);
	tests.checkEqual("consumes the complete chunked terminator", buffer.size(),
		parser.getConsumedBytes());
}

static void testBodyLimitBoundaries(TestRunner& tests)
{
	const std::string headers =
		"POST /upload HTTP/1.1\r\nHost: localhost\r\n";
	{
		RequestParser parser;
		Request request;
		ParseStatus status = continueAfterLocation(parser, request,
			headers + "Content-Length: 5\r\n\r\nhello", 5);
		checkStatus(tests, "accepts Content-Length body exactly at the limit", COMPLETE, status);
		tests.checkEqual("exact-limit body is intact", std::string("hello"),
			request.body);
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = continueAfterLocation(parser, request,
			headers + "Content-Length: 5\r\n\r\n", 4);
		checkStatus(tests, "oversize declared length rejected before body arrives", ERROR, status);
		tests.checkEqual("early oversize rejection is 413",
			PAYLOAD_TOO_LARGE, request.httpStatus);
	}
	{
		RequestParser parser;
		Request request;
		const std::string buffer = headers +
			"Transfer-Encoding: chunked\r\n\r\n"
			"2\r\nhe\r\n3\r\nllo\r\n0\r\n\r\n";
		ParseStatus status =
			continueAfterLocation(parser, request, buffer, 5);
		checkStatus(tests, "accepts chunked body exactly at the total limit", COMPLETE, status);
		tests.checkEqual("exact-limit chunks are intact", std::string("hello"),
			request.body);
	}
	{
		RequestParser parser;
		Request request;
		const std::string buffer = headers +
			"Transfer-Encoding: chunked\r\n\r\n"
			"3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n";
		ParseStatus status =
			continueAfterLocation(parser, request, buffer, 5);
		checkStatus(tests, "rejects chunks whose combined size exceeds the limit", ERROR, status);
		tests.checkEqual("cumulative chunk limit produces 413",
			PAYLOAD_TOO_LARGE, request.httpStatus);
	}
}

static void testLengthValues(TestRunner& tests)
{
	const std::string headers =
		"POST /upload HTTP/1.1\r\nHost: localhost\r\n";
	const std::string invalid[] = {"", "-1", "+1", "1x", std::string(100, '9')};
	const char* reasons[] = {"empty", "negative", "signed", "non-numeric", "overflowing"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
	{
		RequestParser parser;
		Request request;
		ParseStatus status = continueAfterLocation(parser, request,
			headers + "Content-Length: " + invalid[i] + "\r\n\r\n", 100);
		checkStatus(tests, std::string("rejects ") + reasons[i] + " Content-Length", ERROR, status);
		tests.checkEqual(std::string(reasons[i]) + " Content-Length produces 400",
			BAD_REQ, request.httpStatus);
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = continueAfterLocation(parser, request,
			headers + "Content-Length: 0\r\n\r\n", 0);
		checkStatus(tests, "accepts empty Content-Length body with a zero limit", COMPLETE, status);
		tests.checkTrue("zero Content-Length has empty body", request.body.empty());
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = continueAfterLocation(parser, request,
			headers + "Transfer-Encoding: chunked\r\n\r\n0\r\n\r\n", 0);
		checkStatus(tests, "accepts empty chunked body with a zero limit", COMPLETE, status);
		tests.checkTrue("empty chunked body is empty", request.body.empty());
	}
	{
		RequestParser parser;
		Request request;
		ParseStatus status = continueAfterLocation(parser, request,
			headers + "Transfer-Encoding: chunked\r\n\r\n"
			+ std::string(100, 'F') + "\r\n", 100);
		checkStatus(tests, "overflowing chunk size rejected", ERROR, status);
		tests.checkEqual("overflowing chunk size is 400", BAD_REQ, request.httpStatus);
	}
}

static void testBinaryBodyAndChunkRemainder(TestRunner& tests)
{
	const char bytes[] = {'A', '\0', 'B', '\r', '\n'};
	const std::string body(bytes, sizeof(bytes));
	const std::string headers =
		"POST /upload HTTP/1.1\r\nHost: localhost\r\n";
	for (int chunked = 0; chunked < 2; ++chunked)
	{
		const int failuresBefore = tests.failedCount();
		tests.setPrintPass(false);
		RequestParser parser;
		Request request;
		std::string buffer = headers;
		if (chunked)
			buffer += "Transfer-Encoding: chunked\r\n\r\n5\r\n"
				+ body + "\r\n0\r\n\r\n";
		else
			buffer += "Content-Length: 5\r\n\r\n" + body;
		const size_t endOfRequest = buffer.size();
		buffer += "EXTRA";
		ParseStatus status = continueAfterLocation(parser, request, buffer, 5);
		const std::string label = chunked ? "chunked binary" : "Content-Length binary";
		checkStatus(tests, label + ": complete", COMPLETE, status);
		tests.checkEqual(label + ": all bytes preserved", body, request.body);
		tests.checkEqual(label + ": extra bytes not consumed",
			endOfRequest, parser.getConsumedBytes());
		tests.setPrintPass(true);
		reportGroup((chunked ? std::string("Chunked") : std::string("Content-Length"))
			+ " preserves binary body and request boundary",
			tests.failedCount() - failuresBefore);
	}
}

static void testResetAfterBody(TestRunner& tests)
{
	const std::string oldHeaders =
		"POST /old?old=yes HTTP/1.1\r\nHost: old.example\r\n"
		"X-Old: present\r\n";
	const std::string firstRequests[] = {
		oldHeaders + "Content-Length: 3\r\n\r\nold",
		oldHeaders + "Transfer-Encoding: chunked\r\n\r\n3\r\nold\r\n0\r\n\r\n"
	};
	for (size_t i = 0; i < 2; ++i)
	{
		const int failuresBefore = tests.failedCount();
		tests.setPrintPass(false);
		const std::string scenario = i == 0
			? "Content-Length POST -> reset -> GET"
			: "chunked POST -> reset -> chunked POST";
		RequestParser parser;
		Request request;
		ParseStatus status =
			continueAfterLocation(parser, request, firstRequests[i], 10);
		checkStatus(tests, scenario + ": before reset complete", COMPLETE, status);
		parser.reset();
		request.clear();
		tests.checkEqual(scenario + ": reset cursor", size_t(0),
			parser.getConsumedBytes());
		tests.checkTrue(scenario + ": reset clears body", request.body.empty());
		tests.checkTrue(scenario + ": reset clears headers", request.headers.empty());
		tests.checkTrue(scenario + ": reset clears query", request.query.empty());

		std::string second = "GET /new HTTP/1.1\r\nHost: new.example\r\n\r\n";
		std::string expectedBody;
		if (i == 1)
		{
			second = "POST /new HTTP/1.1\r\nHost: new.example\r\n"
				"Transfer-Encoding: chunked\r\n\r\n2\r\nok\r\n0\r\n\r\n";
			expectedBody = "ok";
		}
		status = continueAfterLocation(parser, request, second, 2);
		checkStatus(tests, scenario + ": after reset complete", COMPLETE, status);
		tests.checkEqual(scenario + ": after reset path", std::string("/new"),
			request.path);
		tests.checkEqual(scenario + ": after reset body", expectedBody, request.body);
		tests.checkEqual(scenario + ": after reset Host", std::string("new.example"),
			request.headers["host"]);
		tests.checkTrue(scenario + ": old header absent",
			request.headers.find("x-old") == request.headers.end());
		tests.checkTrue(scenario + ": old query absent", request.query.empty());
		tests.checkEqual(scenario + ": after reset consumed", second.size(),
			parser.getConsumedBytes());
		tests.setPrintPass(true);
		reportGroup("parser can be reused: " + scenario,
			tests.failedCount() - failuresBefore);
	}
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
	testPercentEncoding(tests);
	testReset(tests);
	testEverySplit(tests);
	testChunkTerminatorSplit(tests);
	testBodyLimitBoundaries(tests);
	testLengthValues(tests);
	testBinaryBodyAndChunkRemainder(tests);
	testResetAfterBody(tests);

	return tests.finish();
}

