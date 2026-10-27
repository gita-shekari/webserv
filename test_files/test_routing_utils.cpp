#include "RoutingUtils.hpp"

#include <iostream>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

static void checkTrue(const std::string& name, bool condition)
{
	if (condition)
	{
		g_passed++;
		std::cout << "[PASS] " << name << std::endl;
	}
	else
	{
		g_failed ++;
		std::cout << "[FAIL] " << name << std::endl;
	}
}

static void checkEqual(const std::string& name, const std::string& expected, const std::string& actual)
{
	if (expected == actual)
	{
		g_passed++;
		std::cout << "[PASS] " << name << std::endl;
	}
	else
	{
		g_failed ++;
        std::cerr
            << "[FAIL] " << name
            << "\n  expected: " << expected
            << "\n  actual:   " << actual
            << '\n';
	}
}

/** method at location.methods? methods is empty? lower or upperclass */
static void testIsMethodAllowed()
{
	LocationConfig location;

	checkTrue("empty method list rejects GET", !RoutingUtils::isMethodAllowed(location, "GET"));

	location.methods.push_back("GET");
	location.methods.push_back("POST");

	checkTrue("configed GET allowed", RoutingUtils::isMethodAllowed(location, "GET"));

	checkTrue(
        "configured POST is allowed",
        RoutingUtils::isMethodAllowed(location, "POST")
    );

    checkTrue(
        "unconfigured DELETE is rejected",
        !RoutingUtils::isMethodAllowed(location, "DELETE")
    );

    checkTrue(
        "method comparison is case-sensitive",
        !RoutingUtils::isMethodAllowed(location, "get")
    );
}

/** root empty? path empty? root end with slash? path begin with slash? */
static void testJoinPath()
{
	    checkEqual(
        "join normal root and absolute path",
        "www/index.html",
        RoutingUtils::joinPath("www", "/index.html")
    );

    checkEqual(
        "avoid duplicate slash",
        "www/index.html",
        RoutingUtils::joinPath("www/", "/index.html")
    );

    checkEqual(
        "insert missing slash",
        "www/index.html",
        RoutingUtils::joinPath("www", "index.html")
    );

    checkEqual(
        "keep existing separator",
        "www/index.html",
        RoutingUtils::joinPath("www/", "index.html")
    );

    checkEqual(
        "empty root returns path",
        "/index.html",
        RoutingUtils::joinPath("", "/index.html")
    );

    checkEqual(
        "empty path returns root",
        "www",
        RoutingUtils::joinPath("www", "")
    );

    checkEqual(
        "both empty returns empty",
        "",
        RoutingUtils::joinPath("", "")
    );
}

static void testServerRootPath()
{
	ServerConfig server;
	server.root = "/var/www";

	LocationConfig location;
	location.path = "/images";

	checkEqual(
		"use server root when location root is empty",
        "/var/www/images/logo.png",
		RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/images/logo.png"
        )
	);
}

static void testLocationRootPath()
{
    ServerConfig server;
    server.root = "/var/www";

    LocationConfig location;
    location.path = "/images";
    location.root = "/srv/assets";

    checkEqual(
        "location root replaces server root",
        "/srv/assets/logo.png",
        RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/images/logo.png"
        )
    );

    checkEqual(
        "exact location path maps to location root",
        "/srv/assets",
        RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/images"
        )
    );
}

static void testRootLocationPath()
{
    ServerConfig server;
    server.root = "/var/www";

    LocationConfig location;
    location.path = "/";
    location.root = "/srv/site";

    checkEqual(
        "root location removes leading slash",
        "/srv/site/index.html",
        RoutingUtils::getCorrectFullPath(
            server,
            location,
            "/index.html"
        )
    );
}

static void testGetCorrectFullPath()
{
    testServerRootPath();
    testLocationRootPath();
    testRootLocationPath();
}



static void testIsCGI()
{
    LocationConfig location;
    location.cgi_extension = ".py";

    checkTrue(
        "matching CGI extension",
        RoutingUtils::isCGI(
            location,
            "/cgi/script.py"
        )
    );

    checkTrue(
        "different extension is not CGI",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script.txt"
        )
    );

    checkTrue(
        "path without extension is not CGI",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script"
        )
    );

    checkTrue(
        "dot in directory name is not CGI extension",
        !RoutingUtils::isCGI(
            location,
            "/folder.py/script"
        )
    );

    checkTrue(
        "file extension after dotted directory is CGI",
        RoutingUtils::isCGI(
            location,
            "/folder.py/script.py"
        )
    );

    checkTrue(
        "directory whose name ends in extension is not CGI",
        !RoutingUtils::isCGI(
            location,
            "/folder.py/"
        )
    );

    checkTrue(
        "CGI extension comparison is case-sensitive",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script.PY"
        )
    );

    location.cgi_extension.clear();

    checkTrue(
        "empty CGI configuration rejects script",
        !RoutingUtils::isCGI(
            location,
            "/cgi/script.py"
        )
    );
}

int main()
{
	testIsMethodAllowed();
	testJoinPath();
	testGetCorrectFullPath();
	testIsCGI();

	std::cout << "\n"
			  << g_passed << " passed, "
			  << g_failed << " failed.\n";
	
	// CI use this to check whether it is success or fail.
	return g_failed == 0 ? 0 : 1;
}
