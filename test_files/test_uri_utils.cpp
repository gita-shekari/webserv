#include "UriUtils.hpp"
#include "TestRunner.hpp"

#include <iostream>

static void testValidatePercentEncoding(TestRunner& tests)
{
    tests.checkTrue(
        "plain path is valid",
        UriUtils::validatePercentEncoding("/hello")
    );

    tests.checkTrue(
        "valid uppercase hex encoding",
        UriUtils::validatePercentEncoding("/hello%20world")
    );

    tests.checkTrue(
        "valid lowercase hex encoding",
        UriUtils::validatePercentEncoding("/%2e")
    );

    tests.checkTrue(
        "trailing percent is rejected",
        !UriUtils::validatePercentEncoding("/hello%")
    );

    tests.checkTrue(
        "incomplete encoding is rejected",
        !UriUtils::validatePercentEncoding("/hello%2")
    );

    tests.checkTrue(
        "non-hex encoding is rejected",
        !UriUtils::validatePercentEncoding("/hello%GG")
    );
}

static void testPercentDecode(TestRunner& tests)
{
    std::string output;

    bool success = UriUtils::percentDecode(
        "/hello%20world",
        output
    );

    tests.checkTrue("encoded space is decoded", success);
    tests.checkEqual(
        "decoded path contains space",
        std::string("/hello world"),
        output
    );

    tests.checkTrue(
        "encoded slash is rejected",
        !UriUtils::percentDecode("/hello%2Fworld", output)
    );

    tests.checkTrue(
        "encoded backslash is rejected",
        !UriUtils::percentDecode("/hello%5Cworld", output)
    );
}

static void testValidateDecodedPath(TestRunner& tests)
{
    tests.checkTrue(
        "normal absolute path is valid",
        UriUtils::validateDecodedPath("/images/logo.png")
    );

    tests.checkTrue(
        "root path is valid",
        UriUtils::validateDecodedPath("/")
    );

    tests.checkTrue(
        "empty path is rejected",
        !UriUtils::validateDecodedPath("")
    );

    tests.checkTrue(
        "relative path is rejected",
        !UriUtils::validateDecodedPath("images/logo.png")
    );

    std::string pathWithNul("/hello\0world", 12);

    tests.checkTrue(
        "NUL byte is rejected",
        !UriUtils::validateDecodedPath(pathWithNul)
    );

    std::string pathWithNewline = "/hello";
    pathWithNewline += '\n';
    pathWithNewline += "world";

    tests.checkTrue(
        "newline control character is rejected",
        !UriUtils::validateDecodedPath(pathWithNewline)
    );

    std::string pathWithDel = "/hello";
    pathWithDel += static_cast<char>(127);

    tests.checkTrue(
        "DEL control character is rejected",
        !UriUtils::validateDecodedPath(pathWithDel)
    );
}

static void testNormalizePath(TestRunner& tests)
{
    std::string output;

    bool success = UriUtils::normalizePath(
        "/a//b/./c",
        output
    );

    tests.checkTrue("normalization succeeds", success);
    tests.checkEqual(
        "duplicate slash and dot are removed",
        std::string("/a/b/c"),
        output
    );

    tests.checkTrue(
        "path cannot escape root",
        !UriUtils::normalizePath("/../etc", output)
    );
}

static void testHandleRawTarget(TestRunner& tests)
{

	Request request;
	request.rawTarget =
		"//a/%2e%2e/b?name=hello%20world";

	bool success = UriUtils::handleRawTarget(request);

	tests.checkTrue("raw target is processed", success);
	tests.checkEqual("path is normalized", std::string("/b"), request.path);
	tests.checkEqual(
		"query remains encoded",
		std::string("name=hello%20world"),
		request.query
	);

	Request request_fail;
	request_fail.rawTarget = "/hello%GG";

	tests.checkTrue(
		"invalid encoding rejects raw target",
		!UriUtils::handleRawTarget(request_fail)
	);
}

int	main(void)
{
	TestRunner tests;

	testValidatePercentEncoding(tests);
    testPercentDecode(tests);
    testValidateDecodedPath(tests);
    testNormalizePath(tests);
    testHandleRawTarget(tests);

	return tests.finish();
}