
#include "Response.hpp"
#include "ResponseBuilder.hpp"
#include "Config.hpp"
#include "ConfigParser.hpp"
#include <exception>
int main(int argc, char **argv)
{
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
		std::cout << "Config parsed successfully" << std::endl;

		//Server server(configs[0]);

	}
	catch (const std::exception& e)
	{
		std::cerr << "Config error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
