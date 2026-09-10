#include <iostream>
#include <cstring>
#include "Logger.hpp"

Logger::Level Logger::_minimumLevel = Logger::INFO;

void Logger::setLevel(Level level)
{
	_minimumLevel = level;
}

bool Logger::isEnabled(Level level)
{
	return level >= _minimumLevel;
}

const char* Logger::levelName(Level level)
{
	switch (level)
	{
		case DEBUG: return "DEBUG";
		case INFO: return "INFO";
		case WARNING: return "WARNING";
		case ERROR: return "ERROR";
		case FATAL: return "FATAL";
	}
	return "UNKNOWN";
}

void Logger::log(Level level, const std::string& message)
{
	if (!isEnabled(level))
		return;
	std::cerr << "[" << levelName(level) << "] " << message << std::endl;
}

void Logger::debug(const std::string& message) { log(DEBUG, message); }
void Logger::info(const std::string& message) { log(INFO, message); }
void Logger::warning(const std::string& message) { log(WARNING, message); }
void Logger::error(const std::string& message) { log(ERROR, message); }
void Logger::fatal(const std::string& message) { log(FATAL, message); }

void Logger::systemError(Level level, const std::string& operation, int errorNumber)
{
	std::string message = operation + ": " + std::strerror(errorNumber)
		+ " (errno=" + std::to_string(errorNumber) + ")";
	log(level, message);
}
