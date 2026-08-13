#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <iostream>

class Server
{
	public:
		Server(void);
		~Server(void);

		void start();

		bool	getRunning(void);
		int		getSocketFd(void);

		class ServerException : public std::exception
		{
			const char* what() const throw() override;
		};
	
	private:
		int		_socketFd;
		bool	_isRunning;

};

#endif
