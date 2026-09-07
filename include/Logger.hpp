#pragma once

#include <string>
class Logger
{
	public:
		static void setLevel();
		static void debug();
		static void info();
		static void warning();
		static void error();
		static void fatal();
		static void systemError();
};

