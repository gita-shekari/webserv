#pragma once

enum	e_status
{
	FAILURE,
	SUCCESS
};

class webserv
{
	public:
		webserv();
		~webserv();

		int	init();
		int	run();

	private:
		int	_serverFd;
};