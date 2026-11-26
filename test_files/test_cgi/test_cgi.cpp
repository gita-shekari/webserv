// Run from the repository root, or use: sh test_files/test_cgi/run.sh
#include "CGI.hpp"
#include <cassert>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <iostream>

int finish(CGI& cgi, const std::string& body)
{
	int turns = 0;
	while (cgi.getInputFd() != -1 || cgi.getOutputFd() != -1)
	{
		assert(++turns < 10000);
		if (cgi.getInputFd() != -1 && cgi.getBodyOffset() == body.size())
		{
			close(cgi.getInputFd());
			cgi.setInputFd(-1);
		}
		pollfd fds[2] = {{cgi.getInputFd(), POLLOUT, 0}, {cgi.getOutputFd(), POLLIN, 0}};
		assert(poll(fds, 2, 3000) >= 0);
		assert(fds[0].revents || fds[1].revents || (fds[0].fd == -1 && fds[1].fd == -1));
		if (fds[0].fd != -1 && fds[0].revents)
		{
			ssize_t size = write(fds[0].fd, body.data() + cgi.getBodyOffset(), body.size() - cgi.getBodyOffset());
			if (size > 0)
				cgi.setBodyOffset(cgi.getBodyOffset() + size);
			else if (size == -1 && errno == EPIPE)
			{
				close(cgi.getInputFd());
				cgi.setInputFd(-1);
			}
			else
				assert(errno == EAGAIN || errno == EINTR);
		}
		if (fds[1].fd != -1 && fds[1].revents)
		{
			char buffer[4096];
			ssize_t size = read(fds[1].fd, buffer, sizeof(buffer));
			if (size > 0)
				cgi.appendOutput(buffer, size);
			else if (size == 0)
			{
				close(cgi.getOutputFd());
				cgi.setOutputFd(-1);
			}
			else
				assert(errno == EAGAIN || errno == EINTR);
		}
	}
	int status;
	assert(waitpid(cgi.getPid(), &status, 0) == cgi.getPid());
	cgi.setPid(-1);
	assert(WIFEXITED(status));
	return WEXITSTATUS(status);
}

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);
	alarm(20);
	LocationConfig lc;
	lc.cgi_path = argc > 1 ? argv[1] : "/usr/bin/python3";
	Request request;
	request.method = "GET";
	request.path = "/cgi/echo.py";
	request.rawTarget = "/cgi/echo.py?name=A%20B";
	request.query = "name=A%20B";
	request.version = "HTTP/1.1";
	request.headers["cookie"] = "session=mock";
	request.headers["proxy"] = "ignored";
	CGI cgi;
	assert(!cgi.start(request, lc, "test_files/test_cgi/fixtures/test.php"));
	assert(!cgi.start(request, lc, "test_files/test_cgi/fixtures/test.py.txt"));
	assert(!cgi.start(request, lc, "test_files/test_cgi/fixtures/test"));
	assert(cgi.getPid() == -1 && cgi.getInputFd() == -1 && cgi.getOutputFd() == -1);
	for (int i = 0; i < 3; ++i)
	{
		if (i == 1)
		{
			request.method = "POST";
			request.headers["content-type"] = "application/octet-stream";
			request.headers["transfer-encoding"] = "chunked";
			request.body = std::string(262144, 'b');
			request.body[5] = '\0';
		}
		if (i == 2)
			request.path = "/cgi/";
		assert(cgi.start(request, lc, "test_files/test_cgi/fixtures/echo.py"));
		assert((fcntl(cgi.getInputFd(), F_GETFL) & O_NONBLOCK) != 0);
		assert((fcntl(cgi.getOutputFd(), F_GETFL) & O_NONBLOCK) != 0);
		assert(!cgi.start(request, lc, "test_files/test_cgi/fixtures/echo.py"));
		assert(finish(cgi, request.body) == 0);
		std::string expected = "Content-Type: application/octet-stream\r\n\r\nREQUEST_METHOD=" + request.method
			+ "\nQUERY_STRING=name=A%20B\nCONTENT_LENGTH=" + std::to_string(request.body.size())
			+ "\nCONTENT_TYPE=" + request.headers["content-type"]
			+ "\nSCRIPT_NAME=/cgi/echo.py\nSERVER_PROTOCOL=HTTP/1.1\nHTTP_COOKIE=session=mock\n"
			+ std::string(131072, 'x') + request.body;
		assert(cgi.getOutput() == expected);
		cgi.reset();
	}
	assert(cgi.start(request, lc, "test_files/test_cgi/fixtures/missing.py"));
	assert(finish(cgi, "") == 127);
	cgi.reset();
	lc.cgi_path = "test_files/test_cgi/fixtures/echo.py";
	assert(cgi.start(request, lc, "test_files/test_cgi/fixtures/echo.py"));
	assert(finish(cgi, "") == 127);
	cgi.reset();
	lc.cgi_path.clear();
	assert(!cgi.start(request, lc, "test_files/test_cgi/fixtures/echo.py"));
	assert(cgi.getPid() == -1 && cgi.getInputFd() == -1 && cgi.getOutputFd() == -1);
	std::cout << "PASS: GET, binary POST, directory index, duplex pipes, reuse, missing script, exec failure, invalid config\n";
}
