#include <iostream>
#include <cstring>
#include <cerrno>
#include "Server.hpp"

# define RED "\033[31m"
# define RESET "\033[0m"

void logError(const char* func, int errNb)
{
	std::cerr << RED << "[ERROR] "
			  << func << ": "
			  << std::strerror(errNb)
			  << " (errno = " << errNb << ")" 
			  << RESET << std::endl;
}

