#include "../TestRunner.hpp"
#include "Config.hpp"
#include "Http.hpp"
#include "RoutingUtils.hpp"
#include "Client.hpp"

#include <fstream>
#include <sstream>
#include <string>


static std::string findFixtureRoot()
{
	const char* candidates[] = {
		"test_files/test_routing/fixtures/www",
		"fixtures/www"
	};

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
	{
		struct stat info;
		if (stat(candidates[i], &info) == 0 && S_ISDIR(info.st_mode))
			return candidates[i];
	}
	return "";
}

static LocationConfig baseLocation()
{
	LocationConfig location;
	location.path = "/";
	location.methods.push_back("GET");
	location.methods.push_back("POST");
	location.methods.push_back("DELETE");
	return location;
}

static bool contains(const std::vector<std::string>& entries,
					 const std::string& expected)
{
	return std::find(entries.begin(), entries.end(), expected) != entries.end();
}

//// TEST 1 - Listing 

static void testGetStaicFile(TestRunner& tests, const std::string& fixtureRoot)
{
	// read from fixture and prepare expected static file content.
	std::ifstream file((fixtureRoot + "listing/alpha.txt").c_str(), std::ios::in | std::ios::binary); 
	tests.checkTrue("fixture alpha.txt can be opened", file.is_open());
	if (!file.is_open())
		return ;

	std::ostringstream content;
	content << file.rdbuf();
	const std::string expectedBody = content.str();

	// set up config - matching location as /
	ServerConfig config;
	config.root = fixtureRoot;
	LocationConfig lc = baseLocation();
	config.locations.push_back(lc);

	// Set up fake client
	Client client(-1, 0);
	client.setEffectiveMaxBodySize(config.client_max_body_size);

	// Test recving from client with parsing.
	const std::string raw = 
		"GET /listing/alpha.txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	client.appendReadBuffer(raw.data(), raw.size());

	ParseStatus status = client.parseReadBuffer();
	tests.checkEqual("request asks for location", NEED_LOCATION, status);

	if (status != NEED_LOCATION)
		return ;
	
	// pretend to go matching locaiton
	client.matchLocation(config.locations);
	const size_t index = client.getLocationIndex();
	tests.checkEqual("root location is selected", static_cast<size_t>(0), index);
	if (index != 0)
		return ;
	status = client.parseReadBuffer();
	tests.checkEqual("GET request parsing completes", COMPLETE, status);
	if (status != COMPLETE)
		return ;

	// run routing
	client.setState(REQ_READY);
	client.routing(config);
	tests.checkEqual("routing produces a response", RESPONSE_READY, client.getState());
	if (client.getState() != RESPONSE_READY)
		return ;
	
	client.prepareResponseForSending();
	const std::string wire = client.getWriteBuffer();

	// compare response
	const size_t separator = wire.find("\r\n\r\n");
	tests.checkTrue("response contains header/body separator", separator != std::string::npos);
	if (separator == std::string::npos)
		return ;
	
	const size_t firstLineEnd = wire.find("\r\n");
	const std::string statusLine = wire.substr(0, firstLineEnd);
	const std::string body = wire.substr(separator + 4);
	tests.checkEqual("GET status line", std::string("HTTP/1.1 200 OK"), statusLine);
	tests.checkEqual("GET body equals fixture bytes", expectedBody, body);
	
}

int main()
{
	const std::string fixtureRoot = findFixtureRoot();
	if (fixtureRoot.empty())
	{
		std::cerr << "Cannot find test_files/test_routing/fixtures/www\n";
		return 2;
	}

	TestRunner tests;
	testGetStaicFile(tests, fixtureRoot);

	return tests.finish();
}
