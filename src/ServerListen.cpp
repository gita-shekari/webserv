#include "Server.hpp"
// for socket;
#include <netinet/in.h> 
#include <sys/socket.h>
// for closing socket
#include <unistd.h>

// TODO: to delete -> not used?
void	Server::validateUniquePorts(void) const
{
	for (size_t i = 0; i < _config.size(); ++i)
	{
		for (size_t j = i + 1; j < _config.size(); ++j)
		{
			if (_config[i].port == _config[j].port)
				throw ServerException();
		}
	}
}

void	Server::setListeningSockets(void)
{
	for (size_t i = 0; i < _config.size(); ++i)
	{
		int	fd = createListeningSocket(_config[i]);
		_listeners[fd] = i;
		addPollFds(fd, POLLIN);
	}
	_isRunning = true;
}

int		Server::createListeningSocket(const ServerConfig& config)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	//AF_INET : IPv4 protocol SOCK_STREAM: TCP socket

	if (fd == -1)
	{
		const int saveErr = errno;
		Logger::systemError(Logger::FATAL, "socket creation failed", saveErr);
		/** ERROR TYPES -- for now we only log, not diff them 
		 * ❌ Invalid Arguments (Configuration Errors)
		 * EAFNOSUPPORT (Linux) / WSAEAFNOSUPPORT (Windows): The specified address family (e.g., AF_INET, AF_INET6) is not supported by the OS implementation or network stack.
		 * EPROTONOSUPPORT / WSAEPROTONOSUPPORT: The requested network protocol is not supported within this domain (e.g., trying to use an invalid protocol number for a SOCK_STREAM socket).
		 * EINVAL / WSAEINVAL: General invalid flags or an unknown protocol combination passed into the function parameters. 
		 * 🛑 Resource Exhaustion (System-Level Limits)
		 * EMFILE / WSAEMFILE: 
		 * The process has hit its maximum cap for open file descriptors. Your program has too many open files or sockets and cannot allocate a new one.
		 * ENFILE: The entire operating system has run out of open file allocations globally.
		 * ENOBUFS or ENOMEM / WSAENOBUFS: The kernel lacks insufficient memory or buffer space to provision the new socket. 
		 * 🔒 Permission Constraints
		 * EACCES / WSAEACCES: The process lacks the required privilege to create a socket of this specific type or protocol. (For example: creating raw sockets SOCK_RAW usually requires root/administrative permissions)
		*/
		throw ServerException();
	}

	if (!setNonBlocking(fd))
	{
		close(fd);
		throw ServerException();
	}

	// allow immediate restart after server shutdown.
	#ifdef __APPLE__
	int no_sigpipe = 1;
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
	#endif

	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	//config file contents goes here?
	sockaddr_in serverAddress = {};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(config.port);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(fd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1)
	{
		const int saveErr = errno;
		if (saveErr == EACCES)
		{
			Logger::systemError(Logger::FATAL, "bind: permission denied", saveErr);
		}
		else if (saveErr == EADDRINUSE)
		{
			Logger::systemError(Logger::FATAL, "bind: address already in use", saveErr);
		}
		else
		{
			Logger::systemError(Logger::FATAL, "bind failed", saveErr);
		}
		/** OTHER ERRORs : probably won't get. only log if so.
		 * ❌ EADDRNOTAVAIL / WSAEADDRNOTAVAIL (Cannot assign requested address)
		 * The Cause: You tried to bind the socket to a specific IP address that does not physically belong to any network interface on the local machine (e.g., trying to bind to a public web IP instead of your internal LAN IP).
		 * The Fix: If you want your program to listen on all available network cards, bind using the wildcard address INADDR_ANY (0.0.0.0).
		 * ⚠️ EBADF / WSAENOTSOCK (Bad descriptor)
		 * The Cause: The socket file descriptor passed into bind() is completely invalid. This usually happens if your previous socket() initialization failed (returned -1) and you forgot to check it before calling bind().
		 */
		close(fd);
		throw ServerException();
	}
	
	if (listen(fd, 5) == -1)
	{
		const int saveErr = errno;
		Logger::systemError(Logger::FATAL, "listen failed", saveErr);
		close(fd);
		throw ServerException();
	}

	Logger::info(
	"listening socket established on port="
	+ std::to_string(config.port)
	+ " fd="
	+ std::to_string(fd)
	);

	return fd;
}