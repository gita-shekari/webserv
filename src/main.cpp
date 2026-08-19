#include <fstream>
#include <iostream>
#include <string>
#include "Config.hpp"
#include "Server.hpp"

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	//// take config file & handle 
	//if (argc != 2)
	//{
	//	// ERROR 
	//	std::cout << "too much arguments" << std::endl;
	//	return 1;
	//}
	//std::ifstream configFile(argv[1]);
	//if (!configFile.is_open())
	//{
	//	// System ERROR
	//	std::cerr << "Error opening the configuration file" << std::endl;
	//	return 1;
	//}
	//std::string configContent;
	//Config* config = new Config();
	//while (std::getline(configFile, configContent))
	//{
	//	if (!config->parse(configContent))
	//	{
	//		configFile.close();
	//		delete config;
	//		return 1;
	//	}
	//}
	//configFile.close();

	// Set up server

	Server server;

	if (server.start() == 1)
		return 1;

	return 0;
}