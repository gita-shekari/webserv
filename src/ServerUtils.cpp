#include "Server.hpp"
#include <algorithm>
#include <cerrno>

//TODO: no use
bool Server::getRunning(void)
{
	return this->_isRunning;
}

const char* Server::ServerException::what() const noexcept
{
	return "";
}
