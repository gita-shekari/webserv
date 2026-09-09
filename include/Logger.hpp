#pragma once

#include <string>

class Logger
{
	public:
		enum Level
		{
			DEBUG,
			INFO,
			WARNING,
			ERROR,
			FATAL
		};

		static void setLevel(Level level);
		static bool isEnabled(Level level);

		static void debug(const std::string& message);
		static void info(const std::string& message);
		static void warning(const std::string& message);
		static void error(const std::string& message);
		static void fatal(const std::string& message);
		static void systemError(Level level, const std::string& operation,
								int errorNumber);

	private:
		static Level _minimumLevel;
		static void log(Level level, const std::string& message);
		static const char* levelName(Level level);
};

/** 
 * 1. for anything that can be caused by system resources or system call failures that we cannot handle, use systemError(ERROR, ...);
 * 2. for any fails that is checked by us to prevent code logic bugs, such as double close a fd or find() == end(), we log as fatal(), and stop the server. 
 * 3. for debugging, we can write Log::debug() at places where we want to know there is a client connection, receving or sending information. or EINTR for signal intruption. 
 * 4. for info: any point of a lifecycle. for example server stop, client hangup, extr.
 */