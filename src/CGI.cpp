#include "CGI.hpp"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <limits.h>


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

std::chrono::steady_clock::time_point CGI::getStartedAt() const
{
	return _startedAt;
}

int CGI::getExitStatus() const
{
	return _exitStatus;
}

void CGI::setExitStatus(int status)
{
	_exitStatus = status;
}

void CGI::reset()
{
	// The server closes pipes and reaps the child before resetting this state.
	_exitStatus = -1;
	_pid = -1;
	_inputFd = -1;
	_outputFd = -1;
	_bodyOffset = 0;
	_output.clear();
}
bool CGI::startChild(const Request& request, const LocationConfig& lc, const std::string& scriptPath, int inputPipe[2], int outputPipe[2])
{
	// Resolve both paths before changing to the script's directory.
	char scriptBuffer[PATH_MAX];
	char interpreterBuffer[PATH_MAX];
	if (realpath(scriptPath.c_str(), scriptBuffer) == NULL
		|| realpath(lc.cgi_path.c_str(), interpreterBuffer) == NULL)
	{
		Logger::systemError(Logger::ERROR, "CGI path resolution failed", errno);
		_exit(127);
	}
	std::string script(scriptBuffer);
	std::string interpreter(interpreterBuffer);
	size_t slash = script.find_last_of('/');
	std::string directory = script.substr(0, slash + 1);

	std::string scriptName = request.path;
	if (!scriptName.empty() && scriptName[scriptName.size() - 1] == '/')
		scriptName += script.substr(slash + 1);

	std::vector<std::string> environment;
	environment.push_back("GATEWAY_INTERFACE=CGI/1.1");
	environment.push_back("SERVER_SOFTWARE=webserv");
	environment.push_back("SERVER_PROTOCOL=" + request.version);
	environment.push_back("REQUEST_METHOD=" + request.method);
	environment.push_back("QUERY_STRING=" + request.query);
	environment.push_back("SCRIPT_NAME=" + scriptName);
	environment.push_back("SCRIPT_FILENAME=" + script);
	environment.push_back("PATH_INFO=");
	environment.push_back("REQUEST_URI=" + request.rawTarget);
	// The parser already decoded chunked input; use the actual body size.
	environment.push_back("CONTENT_LENGTH=" + std::to_string(request.body.size()));

	for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
		it != request.headers.end(); ++it)
	{
		if (it->first == "content-type")
		{
			environment.push_back("CONTENT_TYPE=" + it->second);
			continue;
		}
		if (it->first == "content-length" || it->first == "transfer-encoding"
			|| it->first == "connection" || it->first == "proxy")
			continue;
		std::string name = it->first;
		for (size_t i = 0; i < name.size(); ++i)
		{
			if (name[i] == '-')
				name[i] = '_';
			else if (name[i] >= 'a' && name[i] <= 'z')
				name[i] = name[i] - 'a' + 'A';
		}
		environment.push_back("HTTP_" + name + "=" + it->second);
	}

	std::vector<char*> env;
	for (size_t i = 0; i < environment.size(); ++i)
		env.push_back(&environment[i][0]);
	env.push_back(NULL);
	char* args[] = {&interpreter[0], &script[0], NULL};

	if (dup2(inputPipe[0], STDIN_FILENO) == -1
		|| dup2(outputPipe[1], STDOUT_FILENO) == -1)
	{
		Logger::systemError(Logger::ERROR, "CGI dup2 failed", errno);
		_exit(126);
	}
	// Do not let the script retain server sockets or other clients' pipes.
	long maxFd = sysconf(_SC_OPEN_MAX);
	if (maxFd == -1)
	{
		Logger::error("CGI descriptor limit lookup failed");
		_exit(126);
	}
	for (long fd = STDERR_FILENO + 1; fd < maxFd; ++fd)
		close(static_cast<int>(fd));
	if (chdir(directory.c_str()) == -1)
	{
		Logger::systemError(Logger::ERROR, "CGI chdir failed", errno);
		_exit(126);
	}
	execve(interpreter.c_str(), args, &env[0]);
	Logger::systemError(Logger::ERROR, "CGI execve failed", errno);
	_exit(127);
}
bool CGI::start(const Request& request, const LocationConfig& lc, const std::string& scriptPath)
{
	if (_pid != -1 || _inputFd != -1 || _outputFd != -1)
	{
		Logger::error("CGI already started: clean up before starting again");
		return false;
	}
	if (lc.cgi_path.empty() || scriptPath.empty())
	{
		Logger::error("CGI requires an interpreter and a script path");
		return false;
	}
	if (scriptPath.size() < 3 || scriptPath.compare(scriptPath.size() - 3, 3, ".py") != 0)
	{
		Logger::error("CGI only supports .py scripts");
		return false;
	}
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

	// Only the parent's pipe ends are non-blocking; the script uses normal I/O.
	if (fcntl(inputPipe[1], F_SETFL, O_NONBLOCK) == -1
		|| fcntl(outputPipe[0], F_SETFL, O_NONBLOCK) == -1)
	{
		Logger::systemError(Logger::ERROR, "CGI non-blocking setup failed", errno);
		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);
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
		startChild(request, lc, scriptPath, inputPipe, outputPipe);
		_exit(127);
	}

	close(inputPipe[0]);
	close(outputPipe[1]);

	_startedAt = std::chrono::steady_clock::now();
	_exitStatus = -1;
	_inputFd = inputPipe[1];
	_outputFd = outputPipe[0];
	_bodyOffset = 0;
	_output.clear();
	Logger::debug("CGI parent started: pid=" + std::to_string(_pid));
	Logger::debug("CGI input fd=" + std::to_string(_inputFd));
	Logger::debug("CGI output fd=" + std::to_string(_outputFd));
	return true;
}
