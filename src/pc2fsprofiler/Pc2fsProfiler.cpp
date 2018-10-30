#include "pc2fsProfiler/Pc2fsProfiler.h"

/**
 * @file Pc2fsProfiler.cpp
 * @author Markus Maesker, <maesker@gmx.net>
 * @date 31.08.11
 * @class Pc2fsProfiler
 * @version 0.1
 * 
 * @todo The profiler does not work with the gcc optimization flags -O2 and -O3.
 * -O1 is not tested. For profilings dont use these flags.
 * 
 * @brief Profiler to monitor the cpu cycles spent in defined functions.
 * 
 * This class is used to monitor the cpu cycles spent within a defined 
 * environment. \n
 * 
 * Usage:\n
 * 
 * 1. The Pc2fsProfiles is implemented following the singleton pattern. Simply 
 * use \b get_instance in all your threads.\n
 * 2. At the beginning of \b ALL your functions use \b function_start() to start 
 * monitoring this function. \n
 * 3. Use \b function_end() at every code that quits this function. Most likely
 * these are \b return and \b throw commands. You \don't need to call this function
 * if you are calling another function or module that itself uses this 
 * profiler.\n
 * 4. You're done here. Compile and run.\n
 * 5. Run 
@verbatim
  python PG/tools/Scripts/Profiler_Analyser.py /tmp/pc2fs_profiler_core.csv 
@endverbatim
*  The Profilers output file is defined in Pc2fsProfiler.h -> PROFILER_CORE_FILE
* 
* \b Hint:
* 1. Use function_sleep() and function_wakeup() to stop monitoring while waiting 
* for network messages or at mutex. \n
* 
* 2. Be sure to \b always use function_end in monitored functions. The Profiler 
* keeps a Stack that represents the programs trace and is not able to identify 
* unbalanced use of function_start - function_end. The results of such a 
* monitoring will be useless.\n
* \n 
 * What do i get ?
@verbatim [...]
140594596763392,MetadataServer.cpp,get_responsible_journal,e,75821166725280;
140594596763392,MetadataServer.cpp,handle_lookup_inode_number_request,e,75821167328380;
140594596763392,MetadataServer.cpp,metadata_server_worker,x,75821167474640;
140594596763392,MetadataServer.cpp,metadata_server_worker,w,75842490227270;
140594596763392,MetadataServer.cpp,handle_einode_request,s,75842490544530;
140594596763392,MetadataServer.cpp,get_root_inode_fsal_response,s,75842490665710;
140594596763392,MetadataServer.cpp,get_root_inode_fsal_response,e,75842490798490;
140594596763392,MetadataServer.cpp,handle_einode_request,e,75842490909390;
140594596763392,MetadataServer.cpp,metadata_server_worker,x,75842491061110;
[...] @endverbatim
* 
* This is a snipped of the monitoring file pc2fs_profiler_core.csv. It has a 
* very simple structure:
@verbatim
<Threadid>,<Sourcefile>,<function>,<operation>,<cpucycle>;
@endverbatim
* 
* The time spent is acquired by building a program stack of every thread over 
* every file and every function and calculationg the differenz between the stack
* entries cpucycle counts. This is done by the above mentioned python script. A 
* small example of the result is this:
* 
 * @verbatim
= = = = = Thread: 140594596763392 = = = = =
 I + + + + + File: MetadataServer + + + + +
 I (  5.722 %) handle_read_dir_request: 846960
 I ( 12.974 %) get_responsible_journal: 1920380
 I (  1.789 %) get_root_inode_fsal_response: 264860
 I ( 12.159 %) handle_lookup_inode_number_request: 1799760
 I ( 46.557 %) metadata_server_worker: 6891160
 I ( 20.799 %) handle_einode_request: 3078520
 I Total: 14801640 - MetadataServer
 I + + + + + + + + + + + + + + + +
Total: 14801640 - 140594596763392 -
= = = = = = = = = = = = = = = = = = = = = = =

= = = = = Thread: 140594860324704 = = = = =
 I + + + + + File: MetadataServer + + + + +
 I ( 59.427 %) MetadataServer: 1947220
 I (  0.567 %) set_storage_abstraction_layer: 18570
 I (  7.954 %) stop: 260640
 I ( 32.052 %) run: 1050250
 I Total: 3276680 - MetadataServer
 I + + + + + + + + + + + + + + + +
Total: 3276680 - 140594860324704 -
= = = = = = = = = = = = = = = = = = = = = = =

 = = =  Thread usage view  = = =

Thread percentage: ( 81.875 %) - 140594596763392.
Thread percentage: ( 18.125 %) - 140594860324704.

 = = =  System usage view  = = =

Total: 0.01807832 billion ops
@endverbatim

* 
* 
 * */

using namespace std;


/**
 * @brief Return the singleton instance. Creates the instance if called
 * for the first time.
 * */
Pc2fsProfiler *Pc2fsProfiler::instance = NULL;


Pc2fsProfiler::Pc2fsProfiler()
{
    mutex = PTHREAD_MUTEX_INITIALIZER;
    string output = string(PROFILER_CORE_FILE);
    //printf("Creating:%s\n",output.c_str());
    this->_fp.open(output.c_str(), ios::out | ios::app );
}

/** 
 * @brief Close the file.
 * */
Pc2fsProfiler::~Pc2fsProfiler()
{
    _fp.flush();
    _fp.close();
}

/** 
 * @brief Metaprofiler that gets called by the defined makros.
 * @param[in] file __FILE__ 
 * @param[in] method __func__
 * @param[in] type one of 's', 'e', 'x', 'w' defines the type of the operation
 * */
void Pc2fsProfiler::meta_profiler(const char* file,const char* method, const char *type)
{
  #ifdef PROFILER_ENABLED
    pthread_mutex_lock( &mutex );
    ticks tick;
    tick = getticks();
    string output = get_identifier(file,method);

    _fp << output << "," << type << ",";
    _fp << tick << ";" << endl;
    pthread_mutex_unlock( &mutex );
  #endif
}

/** 
 * @brief creates the first part of the entrys string
 * @param[in] file __FILE__ 
 * @param[in] method __func__
 * @return string to write to file
 * */
string Pc2fsProfiler::get_identifier(const char *file ,const char *method)
{
    pthread_t threadid = pthread_self();
    char buf[32];
    memset(&buf[0],'0',32);
    sprintf(&buf[0],"%lu",threadid);
    string output = string(buf);
    output.append(",");
    
    string sfile = string(file);
    size_t found = sfile.find_last_of("/");
    output.append(sfile.substr(found+1));
    output.append(",");
    output.append(method);    
    return output;       
}

/** 
 * @brief get current cpu cycle state
 * */
static __inline__ ticks getticks(void)
{
    unsigned a, d;
    asm("cpuid");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    return (((ticks)a) | (((ticks)d) << 32));
}
