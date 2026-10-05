#include <fstream>
#include <iostream>
#include <string>
#include "ConfigParser.hpp"
#include "Logger.hpp"
#include "Server.hpp"

void printConfig(const std::vector<ServerConfig>& configs)
{
	for (size_t i = 0; i < configs.size(); i++)
	{
		const ServerConfig& sc = configs[i];

		std::cout << "\n==============================" << std::endl;
		std::cout << "SERVER " << i << std::endl;
		std::cout << "==============================" << std::endl;

		std::cout << "host: " << sc.host << std::endl;
		std::cout << "port: " << sc.port << std::endl;
		std::cout << "root: " << sc.root << std::endl;
		std::cout << "max body: "
				  << sc.client_max_body_size
				  << " bytes" << std::endl;

		std::cout << "\nError pages:" << std::endl;

		for (std::map<int, std::string>::const_iterator it =
				 sc.error_pages.begin();
			 it != sc.error_pages.end();
			 ++it)
		{
			std::cout
				<< "  " << it->first
				<< " -> " << it->second
				<< std::endl;
		}

		std::cout << "\nLocations:" << std::endl;

		for (size_t j = 0;
			 j < sc.locations.size();
			 j++)
		{
			const LocationConfig& lc =
				sc.locations[j];

			std::cout
				<< "\n  LOCATION "
				<< lc.path
				<< std::endl;

			std::cout
				<< "    root: "
				<< lc.root
				<< std::endl;

			std::cout
				<< "    index: "
				<< lc.index
				<< std::endl;

			std::cout
				<< "    autoindex: "
				<< (lc.autoindex ? "on" : "off")
				<< std::endl;

			std::cout << "    methods: ";

			for (size_t k = 0;
				 k < lc.methods.size();
				 k++)
			{
				std::cout << lc.methods[k];

				if (k + 1 < lc.methods.size())
					std::cout << ", ";
			}

			std::cout << std::endl;

			if (lc.has_client_max_body_size)
			{
				std::cout
					<< "    max body: "
					<< lc.client_max_body_size
					<< " bytes"
					<< std::endl;
			}

			std::cout
				<< "    upload: "
				<< (lc.upload_enabled
					? "on"
					: "off")
				<< std::endl;

			if (!lc.upload_store.empty())
			{
				std::cout
					<< "    upload_store: "
					<< lc.upload_store
					<< std::endl;
			}

			if (lc.has_redirect)
			{
				std::cout
					<< "    redirect: "
					<< lc.redirect_code
					<< " -> "
					<< lc.redirect_target
					<< std::endl;
			}

			if (!lc.cgi_extension.empty())
			{
				std::cout
					<< "    CGI extension: "
					<< lc.cgi_extension
					<< std::endl;

				std::cout
					<< "    CGI path: "
					<< lc.cgi_path
					<< std::endl;
			}
		}
	}

	std::cout << "\n==============================" << std::endl;
}
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
		printConfig(configs);
		Logger::info("configuration parsed successfully: " + file);
		if (configs.empty())
		{
			Logger::fatal("configuration invariant violated: no server configuration");
			return 1;
		}

		Server server(configs);
		server.start();
	}
	catch (const std::exception& e)
	{
		Logger::fatal(std::string("server startup failed: ") + e.what());
		return 1;
	}
	return 0;
}
