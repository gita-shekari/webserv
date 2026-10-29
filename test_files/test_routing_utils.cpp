#include "RoutingUtils.hpp"
#include "TestRunner.hpp"

#include <iostream>
#include <string>

/** method at location.methods? methods is empty? lower or upperclass */
static void testIsMethodAllowed(TestRunner& tests)
{
	LocationConfig location;

	tests.checkTrue("empty method list rejects GET", !RoutingUtils::isMethodAllowed(location, "GET"));

	location.methods.push_back("GET");
	location.methods.push_back("POST");

	tests.checkTrue("configed GET allowed", RoutingUtils::isMethodAllowed(location, "GET"));

	tests.checkTrue(
        "configured POST is allowed",
        RoutingUtils::isMethodAllowed(location, "POST")
    );

    tests.checkTrue(
        "unconfigured DELETE is rejected",
        !RoutingUtils::isMethodAllowed(location, "DELETE")
    );

    tests.checkTrue(
        "method comparison is case-sensitive",
        !RoutingUtils::isMethodAllowed(location, "get")
    );
}

/** root empty? path empty? root end with slash? path begin with slash? */
static void testJoinPath(TestRunner& tests)
{
	tests.checkEqual(
        "join normal root and absolute path",
        std::string("www/index.html"),
        RoutingUtils::joinPath("www", "/index.html")
    );

    tests.checkEqual(
        "avoid duplicate slash",
        std::string("www/index.html"),
        RoutingUtils::joinPath("www/", "/index.html")
    );

    tests.checkEqual(
        "insert missing slash",
        std::string("www/index.html"),
        RoutingUtils::joinPath("www", "index.html")
    );

    tests.checkEqual(
        "keep existing separator",
        std::string("www/index.html"),
        RoutingUtils::joinPath("www/", "index.html")
    );

    tests.checkEqual(
        "empty root returns path",
        std::string("/index.html"),
        RoutingUtils::joinPath("", "/index.html")
    );

    tests.checkEqual(
        "empty path returns root",
        std::string("www"),
        RoutingUtils::joinPath("www", "")
    );

    tests.checkEqual(
        "both empty returns empty",
        std::string(""),
        RoutingUtils::joinPath("", "")
    );
}

static void testServerRootPath(TestRunner& tests)
{
	ServerConfig server;
	server.root = "/var/www";

	LocationConfig location;
	location.path = "/images";

	tests.checkEqual(
		"use server root when location root is empty",
        std::string("/var/www/images/logo.png"),
		RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/images/logo.png"
        )
	);
}

static void testLocationRootPath(TestRunner& tests)
{
    ServerConfig server;
    server.root = "/var/www";

    LocationConfig location;
    location.path = "/images";
    location.root = "/srv/assets";

    tests.checkEqual(
        "location root replaces server root",
        std::string("/srv/assets/logo.png"),
        RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/images/logo.png"
        )
    );

    tests.checkEqual(
        "exact location path maps to location root",
        std::string("/srv/assets"),
        RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/images"
        )
    );
}

static void testRootLocationPath(TestRunner& tests)
{
    ServerConfig server;
    server.root = "/var/www";

    LocationConfig location;
    location.path = "/";
    location.root = "/srv/site";

    tests.checkEqual(
        "root location removes leading slash",
        std::string("/srv/site/index.html"),
        RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/index.html"
        )
    );
}

static void testGetCorrectFullPath(TestRunner& tests)
{
    testServerRootPath(tests);
    testLocationRootPath(tests);
    testRootLocationPath(tests);
}



static void testIsCGI(TestRunner& tests)
{
    LocationConfig location;
    location.cgi_extension = ".py";

    tests.checkTrue(
        "matching CGI extension",
        RoutingUtils::isCGI(
            location,
            "/cgi/script.py"
        )
    );

    tests.checkTrue(
        "different extension is not CGI",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script.txt"
        )
    );

    tests.checkTrue(
        "path without extension is not CGI",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script"
        )
    );

    tests.checkTrue(
        "dot in directory name is not CGI extension",
        !RoutingUtils::isCGI(
            location,
            "/folder.py/script"
        )
    );

    tests.checkTrue(
        "file extension after dotted directory is CGI",
        RoutingUtils::isCGI(
            location,
            "/folder.py/script.py"
        )
    );

    tests.checkTrue(
        "directory whose name ends in extension is not CGI",
        !RoutingUtils::isCGI(
            location,
            "/folder.py/"
        )
    );

    tests.checkTrue(
        "CGI extension comparison is case-sensitive",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script.PY"
        )
    );

    location.cgi_extension.clear();

    tests.checkTrue(
        "empty CGI configuration rejects script",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script.py"
        )
    );
}

int main()
{
	TestRunner tests;

	testIsMethodAllowed(tests);
	testJoinPath(tests);
	testGetCorrectFullPath(tests);
	testIsCGI(tests);
	
	// CI use this to check whether it is success or fail.
	return tests.finish();
}
