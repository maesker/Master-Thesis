#ifndef _COMMUNICATIONLOGGER_
#define _COMMUNICATIONLOGGER_

//#define LOGGING_ENABLED
#define LOGFILE "Communication.log"
#define VISUALOUTPUT false

#include "logging/Logger.h"
#include <pthread.h>

#define COMMUNICATIONLOGLEVEL LOG_LEVEL_ERROR

class CommunicationLogger
{
public:
    static Logger* get_instance();

private:
    static Logger* instance;
    static bool initialized;
    static pthread_mutex_t mutex_init;
};


#endif //_COMMUNICATIONLOGGER_
