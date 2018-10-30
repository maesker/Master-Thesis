#include "GaneshaProfiler.h"

/**
* @file GaneshaProfiler.c
* @author Markus Maesker, <maesker@gmx.net>
* @date 08.09.11
*/


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


/** 
 * @brief Metaprofiler that gets called by the defined makros.
 * @param[in] file __FILE__ 
 * @param[in] method __func__
 * @param[in] type one of 's', 'e', 'x', 'w' defines the type of the operation
 * 
 * @return void
 * 
 * The function opens the file to append an profile entry. It is protected by a 
 * static mutex.
 * */
void meta_profiler(const char* file_path,const char* method, const char *type)
{
    #ifdef PROFILER_ENABLED
    
    pthread_mutex_lock( &mutex );
    FILE *file;
    file = fopen(GANESHA_PROFILER_CORE_FILE, "a+");
    size_t size = strlen(file_path)+strlen(method)+strlen(type)+64;
    char buf[size];
    ticks tick;
    tick = getticks();
    pthread_t threadid = pthread_self();
    snprintf(&buf[0],size,"%lu,%s,%s,%s,%lu;\n",threadid,file_path,method,type,tick);
    fputs(buf ,file);
    fflush(file);
    fclose(file);
    pthread_mutex_unlock( &mutex );
    
    #endif
}

/** 
 * @brief get current cpu cycle count 
 * */
static __inline__ ticks getticks(void)
{
    unsigned a, d;
    asm("cpuid");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    return (((ticks)a) | (((ticks)d) << 32));
}
