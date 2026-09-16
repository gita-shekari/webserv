#include <fstream>
#include <iostream>
#include <string>
#include "ConfigParser.hpp"
#include "Logger.hpp"
#include "Server.hpp"

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if(argc > 2)
	{
		std::cerr << "Usage: ./webserv <config_file>" << std::endl;
		return 1;
	}
	std::string file;
	if (argc == 2)
		file = argv[1];
	else
		file = "config/default.conf";
	try
	{
		ConfigParser parser;
		std::vector<ServerConfig> configs = parser.parseConfig(file);
		Logger::info("configuration parsed successfully: " + file);
		if (configs.empty())
		{
			Logger::fatal("configuration invariant violated: no server configuration");
			return 1;
		}

		Server server(configs[0]);
		server.start();
	}
	catch (const std::exception& e)
	{
		Logger::fatal(std::string("server startup failed: ") + e.what());
		return 1;
	}
	return 0;
}
