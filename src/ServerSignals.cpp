#include "Server.hpp"
#include <stdexcept>

volatile std::sig_atomic_t Server::_stopRequested = 0;

void	Server::handleStopSignal(int)
{
	_stopRequested = 1;
}

void	Server::installSignals()
{
	_stopRequested = 0;

	if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR
		|| std::signal(SIGINT, handleStopSignal) == SIG_ERR
		|| std::signal(SIGTERM, handleStopSignal) == SIG_ERR)
	{
		throw std::runtime_error("failed to install server signal handlers");
	}
}

bool	Server::stopRequested()
{
	return _stopRequested != 0;
}
