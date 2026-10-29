#pragma once

#include <string>
#include <sys/types.h>
#include "Http.hpp"
#include "Config.hpp"
#include <Logger.hpp>
#include <unistd.h>

class CGI
{
	public:
		CGI();

		pid_t	getPid() const;
		int		getInputFd() const;
		int		getOutputFd() const;
		size_t	getBodyOffset() const;
		const std::string& getOutput() const;

		void	setPid(pid_t pid);
		void	setInputFd(int fd);
		void	setOutputFd(int fd);
		void	setBodyOffset(size_t offset);
		void	appendOutput(const char* data, size_t size);
		void	clearOutput();
		void	reset();
		bool	startChild(const Request& request, const LocationConfig& lc, const std::string& scriptPath, int inputPipe[2], int outputPipe[2]);
		bool	start(const Request& request, const LocationConfig& lc, const std::string& scriptPath);

	private:
		pid_t		_pid;
		int			_inputFd;
		int			_outputFd;
		size_t		_bodyOffset;
		std::string	_output;
};
