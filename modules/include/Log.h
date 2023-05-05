#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "DebugBreak.h"

class Log
{
public:
	static void log(const char* format, ...)
	{
		printPrefix("log");

		va_list argptr;
		va_start(argptr, format);
		vfprintf(stdout, format, argptr);
		va_end(argptr);
	}

	static void warning(const char* format, ...)
	{
		printPrefix("warning");

		va_list argptr;
		va_start(argptr, format);
		vfprintf(stdout, format, argptr);
		va_end(argptr);
	}

	static void error(const char* format, ...)
	{
		printPrefix("error");

		va_list argptr;
		va_start(argptr, format);
		vfprintf(stdout, format, argptr);
		va_end(argptr);
	}

	[[noreturn]] static void fatal(const char* format, ...)
	{
		printPrefix("fatal");

		va_list argptr;
		va_start(argptr, format);
		vfprintf(stdout, format, argptr);
		va_end(argptr);

		DEBUG_BREAK();

		exit(1);
	}

private:
	static void printPrefix(const char* prefix)
	{
		printf("[%s] ", prefix);
	}
};
