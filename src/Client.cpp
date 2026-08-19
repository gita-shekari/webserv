#include "Client.hpp"


Client::Client():_fd(-1)
{
}

Client::~Client()
{
}

int Client::getFd() const
{
	return _fd;
}
