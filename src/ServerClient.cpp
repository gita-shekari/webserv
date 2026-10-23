#include "Server.hpp"

void	Server::acceptNewClient(int listenerFd)
{
	//struct sockaddr_in clientAddr;
	//socklen_t len = sizeof(clientAddr);
	std::map<int, size_t>::iterator listenerIt = _listeners.find(listenerFd);
	if (listenerIt == _listeners.end())
	{
		Logger::fatal("acceptNewClient called with unknown listener fd");
		throw ServerException();
	}

	size_t	acceptedCount = 0;

	while (acceptedCount < MAX_ACCEPTS_PER_EVENT)
	{
		int clientFd = accept(listenerFd, NULL, NULL);

		if (clientFd == -1)
		{
			const int saveErr = errno;
			if (saveErr == EAGAIN || saveErr == EWOULDBLOCK)
				return; // no more pending clients in the accept queue
			if (saveErr == EINTR)
			{
				Logger::debug("accept interrupted; retrying");
				continue;
			}
			if (saveErr == EMFILE || saveErr == ENFILE)
			{
				Logger::systemError(Logger::ERROR, "accept: file descriptor limit reached", saveErr);
				return;
			}
			if (saveErr == EBADF || saveErr == ENOTSOCK)
			{
				Logger::systemError(Logger::FATAL, "accept: invalid listening socket", saveErr);
				throw ServerException();
			}
			Logger::systemError(Logger::WARNING, "accept failed", saveErr);
			return;
		}

		if (!setNonBlocking(clientFd))
		{
			close(clientFd);
			continue;
		}

		addPollFds(clientFd, POLLIN);

		size_t	serverIndex = listenerIt->second;
		_clients[clientFd] = Client(clientFd, serverIndex);

		acceptedCount++;
		Logger::debug("client connected: fd=" + std::to_string(clientFd));
	}
}

bool	Server::receiveClientData(Client& client)
{
	char buffer[1024];

	int fd = client.getFd();
	ssize_t bytesReceived = recv(fd, buffer, sizeof(buffer), 0);

	if (bytesReceived > 0)
	{
		client.updateLastActivity();
		Logger::debug("received client data: fd=" + std::to_string(fd)
			+ " bytes=" + std::to_string(bytesReceived));

		client.appendReadBuffer(buffer, static_cast<size_t>(bytesReceived));

		const ServerConfig& serverConfig = _config[client.getServerIndex()];

		//set server-level default only once for this request
		if (client.getEffectiveMaxBodySize() == static_cast<size_t>(-1))
			client.setEffectiveMaxBodySize(serverConfig.client_max_body_size);

		ParseStatus status = client.parseReadBuffer();

		if (status == NEED_LOCATION)
		{
			client.matchLocation(serverConfig.locations);
			size_t	locationIndex = client.getLocationIndex();
			if (locationIndex != static_cast<size_t>(-1))
			{
				const LocationConfig& location = serverConfig.locations[locationIndex];
				//update maxBodySize to location config level
				if (location.has_client_max_body_size)
					client.setEffectiveMaxBodySize(location.client_max_body_size);
			}
			status = client.parseReadBuffer();
		}
		if (status == INCOMPLETE)
			return false;

		if (status == COMPLETE || status == ERROR)
		{
			client.setState(REQ_READY);
			return true;
		}
		return false;
	}
	else if (bytesReceived == 0)
	{
		Logger::debug("client closed connection: fd=" + std::to_string(fd));
		markForClose(fd);
		return false;
	}
	else
	{
		const int saveErr = errno;
		if (saveErr == EAGAIN || saveErr == EWOULDBLOCK)
			return false;
		if (saveErr == ECONNRESET || saveErr == ETIMEDOUT)
		{
			markForClose(fd);
			Logger::info("recv: client connection ended");
		}
		else if (saveErr == EINTR)
			Logger::debug("recv interrupted; retry on the next event");
 		else
		{
			Logger::systemError(Logger::ERROR, "recv failed", saveErr);
			markForClose(fd);
		}
		return false;
	}
}

bool Server::sendClientData(Client& client)
{
	int fd = client.getFd();
	Logger::debug("response ready to send: fd=" + std::to_string(fd));

	const std::string& buffer = client.getWriteBuffer();
	ssize_t offset = client.getWriteOffSet();

	ssize_t bytesSent = send(fd, buffer.data() + offset, buffer.size() - offset, 0);

	if (bytesSent > 0)
	{
		client.updateLastActivity();
		client.setWriteOffSet(offset + bytesSent);
		if (static_cast<size_t>(client.getWriteOffSet()) == buffer.size())
			return true;
		return false;
	}
	if (bytesSent == -1)
	{
		int saveErr = errno;

		if (saveErr == EAGAIN || saveErr == EWOULDBLOCK || saveErr == EINTR)
			return false;
		Logger::systemError(Logger::ERROR, "send failed", saveErr);
		markForClose(fd);
	}
	return false;
}
