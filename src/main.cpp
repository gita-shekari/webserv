#include "Server.hpp"

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	Server server(8080);
	server.start();
	
	return 0;
}
