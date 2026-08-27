#include "webserv.hpp"

int	main()
{
	webserv	server;

	if (!server.init())
		return 1;
	server.run();
	return 0;
}