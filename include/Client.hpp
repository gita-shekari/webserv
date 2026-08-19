#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include <stdexcept>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <unistd.h>

class Client
{
	private:
		int			_fd;
	public:
		Client();
		~Client();
		int getFd() const;
};

#endif


