/*
* Logger.cpp
*
* Enables modules to log events to files.
*
* Sebastian Moors <mauser@smoors.de>
*
*/

#include <sstream>

#include "time.h"
#include "logging/Logger.h"

using namespace std;

Logger::Logger()
{
    log_file.open("/tmp/default.log", ios::out | ios::app );
    log_file_mutex = PTHREAD_MUTEX_INITIALIZER;
    log_level = LOG_LEVEL_DEBUG;
    logger_started = 1;
}

Logger::~Logger()
{
    log_file.flush();
    log_file.close();
    pthread_mutex_destroy(&log_file_mutex);
}

int16_t Logger::verify()
{
    if (logger_started)
    {
        return 1;
    }
    return 0;
}

/**
* Define the location of the log file
* @param location The filename of the logfile
* */


void Logger::set_log_location(string location)
{
    pthread_mutex_lock(&log_file_mutex);

    #ifdef DEBUG_LOGGER_MUTEX
    cout << "set_log_location: waiting for location mutex" << endl;
    #endif

    
    #ifdef DEBUG_LOGGER_MUTEX
    cout << "set_log_location: got location mutex" << endl;
    #endif

    log_location = location;
    if(log_file.is_open()) log_file.close();
    
    log_file.open( location.c_str() , ios::out | ios::app );

    pthread_mutex_unlock(&log_file_mutex);
    
    #ifdef DEBUG_LOGGER_MUTEX
    cout << "set_log_location: unlocked location mutex" << endl ;
    #endif
}

/**
* Decide if the log messages should be also printed to stdout
* @param value A boolean value
* */

void Logger::set_console_output(bool value)
{
    is_console_output_enabled = value;
}

/**
* Decides which values should be logged.
* @param level 0: Log everything 1:Log only errors 2: Log only warnings
* */

void Logger::set_log_level(int level)
{
    log_level = level;
}


/**
* The logging function that does the 'real' works. debug_log, warning_log and error_log are only macros
* which call meta_log.
*
* @param level The log level
* @param level The message which should be logged
* @param level The method which issued the log message
* @param level The line in which the log statement was issued
* */

void Logger::meta_log(int level, const char *format, const char* method, int line,...)
{
    #ifdef LOGGER_ENABLED

    if ( level < log_level)
    {
        return;
    }
    stringstream log_stream;
    string log_message;

    char *buffer = (char *) malloc(512);
    if (buffer!=NULL)
    {
        va_list args;
        va_start (args, format);
        vsprintf (buffer,format, args);
        va_end (args);

        time_t rawtime;
        struct tm * timeinfo;
        char tbuffer [18];
        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        strftime (tbuffer,18,"%y-%m-%d,%H:%M:%S",timeinfo);

        log_stream << "[" << tbuffer << "] " << method << ": " << buffer;
        log_message = log_stream.str();

        #ifdef DEBUG_LOGGER_MUTEX
        cout << "meta_log: waiting location mutex" << endl ;
        #endif

        pthread_mutex_lock(&log_file_mutex);

        if(is_console_output_enabled)
        {
            cout << buffer << endl;
        }

        if(log_file.is_open())
        {
            log_file << log_message << endl;
        }

        pthread_mutex_unlock(&log_file_mutex);

        #ifdef DEBUG_LOGGER_MUTEX
        cout << "meta_log: unlocking location mutex" << endl ;
        #endif  

        free(buffer);
    }

   #endif
}

/*
int main()
{
Logger *l = new Logger();
l->set_console_output(true);
l->set_log_level(2);
l->debug_log("this is a log message");
delete l;
}*/
