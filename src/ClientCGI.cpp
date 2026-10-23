#include "Client.hpp"

// request
// → detect CGI
// → create 2 pipes
// → fork()
// → child redirects stdin/stdout
// → child execve() the interpreter + script
// → parent communicates with child
// → build HTTP response

void Client::handleCGI(const ServerConfig& serverConfig, const LocationConfig& lc, const std::string& scriptPath)
{
	int inputPipe[2];
	int outputPipe[2];

	if (pipe(inputPipe) == -1)
	{
		Logger::error("CGI input pipe creation failed");
		return handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
	}

	if (pipe(outputPipe) == -1)
	{
		Logger::error("CGI output pipe creation failed");
		close(inputPipe[0]);
		close(inputPipe[1]);
		return handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
	}

	pid_t pid = fork();

	if (pid == -1)
	{
		Logger::error("CGI fork failed");
		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);
		return handleErrorResponse(serverConfig, INTERNAL_SERVER_ERR);
	}
	if (pid == 0)
	{
		if (dup2(inputPipe[0], STDIN_FILENO) == -1)
			_exit(1);

		if (dup2(outputPipe[1], STDOUT_FILENO) == -1)
			_exit(1);

		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);
		size_t slash = scriptPath.find_last_of('/');
		std::string scriptDir = ".";
		std::string scriptName = scriptPath;
		if (slash != std::string::npos)
		{
			scriptDir = scriptPath.substr(0, slash);
			scriptName = scriptPath.substr(slash + 1);
		}
		Logger::debug("CGI script directory: " + scriptDir);
		Logger::debug("CGI script name: " + scriptName);
		Logger::debug("CGI interpreter: " + lc.cgi_path);
		if (chdir(scriptDir.c_str()) == -1)
			_exit(1);
		Logger::debug("CGI working directory changed successfully");
		char* argv[3];
		argv[0] = const_cast<char*>(lc.cgi_path.c_str());
		argv[1] = const_cast<char*>(scriptName.c_str());
		argv[2] = NULL;
	}
	else
	{
		Logger::debug("parent");
		//_pid = pid;
		_state = CGI_RUNNING;

	}
}


//checkPIDprocess()
//{
	// result = waitpid()
//	if (result > 0)
//	{
//      readRemainingCGIData(pipefd)
//		buildCGIResponse();
//		_state = RESPONSE_READY;
//		close(pipeFd);
//		find it in server._pollFd, and make it = -1
//		remove from server._pipeFds
//	}
//}