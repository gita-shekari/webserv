#include <fstream>
#include <iostream>
#include <string>
#include "Config.hpp"
#include "Server.hpp"

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (argc != 2)
	{
		std::cout << "provide one configuration file" << std::endl;
		return 1;
	}

	try
	{
		Config configFile;
		configFile.tokenization(argv[1]);
		std::cout << "in config parsing" << std::endl;
		std::vector<struct Token>& tokens = configFile.getTokens();
		for (const auto& token : tokens)
		{

			std::cout << token.type << std::endl;
			std::cout << token.value << std::endl;
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}
	

	//// take config file & handle 

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