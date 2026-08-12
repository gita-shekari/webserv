#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>

class Client
{
	private:
		int _fd;
		std::string _inputBuffer;
		std::string _outputBuffer;

	public:
		Client(int fd);
		~Client();

		int getFd() const;
};

#endif
