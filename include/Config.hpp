#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

enum	TokenType
{
	WORD,
	L_BRACE,
	R_BRACE,
	COLUM
};

struct Token
{
	TokenType	type;
	std::string	value;
};

struct ServerConfig
{
	std::string	host = "";
	int			port = 0;
	std::string	serverName = "localhost";
	//std::string	root = "./www";
	//std::string	index = "index.html";
};


class Config
{
	public:
		void tokenization(char *configFilePath);

		std::vector<struct Token>& getTokens() {
			return _tokens;
		}

		class ConfigError : public std::exception
		{
			public:
				explicit ConfigError(const std::string& msg) : _message(msg) {};
				const char* what() const noexcept override;

			private:
				std::string _message;
		};

	private:
		std::vector<struct Token> _tokens;
		struct ServerConfig _configFile;

};


#endif
