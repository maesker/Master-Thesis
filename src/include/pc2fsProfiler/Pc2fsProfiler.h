
#ifndef pc2fsPROFILER_H_
#define pc2fsPROFILER_H_

/**
* @file Pc2fsProfiler.h
* @author Markus Maesker, <maesker@gmx.net>
* @date 31.08.11
*/

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sstream>
#include <pthread.h>

using namespace std;

/** 
 * @def function_start
 * @brief Starts to count cycles of the function this function is called from
 * */
#define function_start() meta_profiler(__FILE__ ,__func__,"s")

/** 
 * @def function_end
 * @brief Ends counting cycles of the function this function is called from
 * */
#define function_end()   meta_profiler(__FILE__ ,__func__,"e")

/** 
 * @def function_sleep
 * @brief Stops counting cycles within a function. 
 * 
 * Use this in front of mutex, network connections, and so on. Call this 
 * function an the cycles spend while waiting for a mutex, or a network message
 * are not monitored
 * */
#define function_sleep() meta_profiler(__FILE__ ,__func__,"x")

/** 
 * @def function_wakeup
 * @brief Starts counting cycles again.
 * 
 * Use this to continue monitoring a function that previously was suspended.
 * */
#define function_wakeup() meta_profiler(__FILE__ ,__func__,"w")


/** 
 * @typedef ticks
 * @brief used for the cpy cycles
 * */
typedef unsigned long long ticks;

/** 
 * @def PROFILER_CORE_FILE
 * @brief Path to write the output to.
 * */
#define PROFILER_CORE_FILE "/tmp/pc2fs_profiler_core.csv"

static __inline__ ticks getticks(void);

class Pc2fsProfiler
{
    public:
        Pc2fsProfiler();
        ~Pc2fsProfiler();
        void meta_profiler(const char* file,const char* method, const char *type);
        
        static Pc2fsProfiler* get_instance()
        {
            if(instance == NULL)
                instance = new Pc2fsProfiler();
            return instance;
        }
        
    private:
        mutable pthread_mutex_t mutex; /* < mutex */
        ofstream _fp;
        static Pc2fsProfiler* instance;
        string get_identifier(const char* file,const char *method);
       
};


#endif
