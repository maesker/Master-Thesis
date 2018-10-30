/*
* Logger.cpp
*
* Enables modules to log events to files.
*
* Sebastian Moors <mauser@smoors.de>
*
*/
#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <fstream>
#include <stdarg.h>

using namespace std;

enum LOG_LEVEl{ LOG_LEVEL_DEBUG=1, LOG_LEVEL_WARNING=2, LOG_LEVEL_ERROR=3 };

#define debug_log(x,...)    meta_log(LOG_LEVEL_DEBUG,x, __PRETTY_FUNCTION__, __LINE__,##__VA_ARGS__)
#define error_log(x,...)    meta_log(LOG_LEVEL_ERROR,x, __PRETTY_FUNCTION__, __LINE__,##__VA_ARGS__)
#define warning_log(x,...)  meta_log(LOG_LEVEL_WARNING,x, __PRETTY_FUNCTION__, __LINE__,##__VA_ARGS__)

/**
* @class Logger
*
* @brief This class provides logging features.
*
* The Logger class is used to provide logging features
* to all other modules. The Logger class is not a singleton
* and each object can be configured to log to an individiual
* destination file.
*
* @author Sebastian Moors
*
* $Header $
**/


class Logger
{

public:
	Logger();
	~Logger();

	void set_console_output(bool);
	void set_log_level(int);
	void set_log_location(string);
	void meta_log(int level, const char *format, const char* method, int line,...);
	int16_t verify();

private:
	bool is_console_output_enabled;
	bool logger_started;

	//makes sure that there are no conflicting writes to the same file
	pthread_mutex_t log_file_mutex;

	ofstream log_file;
	string log_location;

	int log_level;
};

#endif //LOGGER_H_
