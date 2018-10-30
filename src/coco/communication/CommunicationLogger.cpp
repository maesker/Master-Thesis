#include "coco/communication/CommunicationLogger.h"

using namespace std;

bool CommunicationLogger::initialized = true;
Logger* CommunicationLogger::instance = new Logger;
pthread_mutex_t CommunicationLogger::mutex_init = PTHREAD_MUTEX_INITIALIZER;

Logger* CommunicationLogger::get_instance()
{
    if (!initialized)
    {
        int failure = pthread_mutex_lock(&mutex_init);
        if (failure)
        {
            return NULL;
        }
        if (!initialized)
        {
            //set up Logger, set up need to be done only once
            (*instance).set_log_location(LOGFILE);
            initialized = true;
            (*instance).set_console_output(VISUALOUTPUT);
            (*instance).set_log_level(COMMUNICATIONLOGLEVEL);
        }
        pthread_mutex_unlock(&mutex_init);
    }
    return instance;
}

