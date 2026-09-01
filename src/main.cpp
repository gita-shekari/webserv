
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
	try
	{
		ConfigParser cp;
		Config configs = cp.parseConfig(argv[1]);
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}

	return 0;
}
