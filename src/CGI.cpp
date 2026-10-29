#include "CGI.hpp"


CGI::CGI()
	: _pid(-1),
	  _inputFd(-1),
	  _outputFd(-1),
	  _bodyOffset(0)
{
}

pid_t CGI::getPid() const
{
	return _pid;
}

int CGI::getInputFd() const
{
	return _inputFd;
}

int CGI::getOutputFd() const
{
	return _outputFd;
}

size_t CGI::getBodyOffset() const
{
	return _bodyOffset;
}

const std::string& CGI::getOutput() const
{
	return _output;
}

void CGI::setPid(pid_t pid)
{
	_pid = pid;
}

void CGI::setInputFd(int fd)
{
	_inputFd = fd;
}

void CGI::setOutputFd(int fd)
{
	_outputFd = fd;
}

void CGI::setBodyOffset(size_t offset)
{
	_bodyOffset = offset;
}

void CGI::appendOutput(const char* data, size_t size)
{
	_output.append(data, size);
}

void CGI::clearOutput()
{
	_output.clear();
}

void CGI::reset()
{
	_pid = -1;
	_inputFd = -1;
	_outputFd = -1;
	_bodyOffset = 0;
	_output.clear();
}
// bool CGI::startChild(const Request& request, const LocationConfig& lc, const std::string& scriptPath, int inputPipe[2], int outputPipe[2])
// {

// }
bool CGI::start(const Request& request, const LocationConfig& lc, const std::string& scriptPath)
{
	(void)scriptPath;
	(void)lc;
	(void)request;
	int inputPipe[2];
	int outputPipe[2];

	if (pipe(inputPipe) == -1)
	{
		Logger::error("CGI input pipe creation failed");
		return false;
	}

	if (pipe(outputPipe) == -1)
	{
		Logger::error("CGI output pipe creation failed");
		close(inputPipe[0]);
		close(inputPipe[1]);
		return false;
	}

	_pid = fork();

	if (_pid == -1)
	{
		Logger::error("CGI fork failed");
		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);
		return false;
	}

	if (_pid == 0)
	{
		//startChild(request, lc, scriptPath, inputPipe, outputPipe);
		return false;
	}

	close(inputPipe[0]);
	close(outputPipe[1]);

	_inputFd = inputPipe[1];
	_outputFd = outputPipe[0];
	_bodyOffset = 0;
	_output.clear();

	Logger::debug("CGI parent started: pid=" + std::to_string(_pid));
	Logger::debug("CGI input fd=" + std::to_string(_inputFd));
	Logger::debug("CGI output fd=" + std::to_string(_outputFd));

	return true;
}
