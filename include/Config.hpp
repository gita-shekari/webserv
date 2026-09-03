#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

//enum	TokenType
//{
//	WORD,
//	L_BRACE,
//	R_BRACE,
//	COLUM
//};

//struct Token
//{
//	TokenType	type;
//	std::string	value;
//};

struct LocationConfig
{
	std::string path;
	std::vector<std::string> methods;
	std::string root;
	std::string index;
};

struct ServerConfig
{
	int port;
	std::string root;
	std::string index;
	std::vector<LocationConfig> locations;
};


//class Config
//{
//	public:
//		void tokenization(char *configFilePath);

//		std::vector<struct Token>& getTokens() {
//			return _tokens;
//		}

//		class ConfigError : public std::exception
//		{
//			public:
//				explicit ConfigError(const std::string& msg) : _message(msg) {};
//				const char* what() const noexcept override;

//			private:
//				std::string _message;
//		};

//	private:
//		std::vector<struct Token> _tokens;
//		struct ServerConfig _configFile;

//};
