/**
 * @file LBConf.h
 *
 * @date 20 July 2011
 * @author Denis Dridger
 *
 *
 * @brief This class reads configuration values from a file 
 * and provides them to the loadbalancing managers.
 */


#ifndef LBCONF_H_
#define LBCONF_H_


#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "pc2fsProfiler/Pc2fsProfiler.h"


//provide a convenient access config values 
#define SECONDS_UNTIL_NEXT_REBALANCE LBConf::seconds_until_next_rebalance
#define CPU_THRESHOLD LBConf::cpu_threshold
#define W_SWAPS LBConf::w_swaps
#define W_THREADS LBConf::w_threads
#define W_CPU LBConf::w_cpu
#define LB_CONSOLE_OUTPUT LBConf::console_output
#define LB_LOGLEVEL LBConf::loglevel
#define LB_LOGFILE LBConf::logfile
#define MAX_THREADS LBConf::max_no_of_threads
#define MAX_SWAPS LBConf::max_no_of_swaps
#define THREAD_THRESHOLD LBConf::thread_threshold
#define SWAPS_THRESHOLD LBConf::thread_threshold
#define CPU_LOAD_TIMESPAN LBConf::cpu_load_timespan
#define LOAD_BALANCER_ENABLED LBConf::load_balancer_enabled
#define AUTO_LOAD_BALANCER_ENABLED LBConf::automatic_load_balancer_enabled
#define ALMOST_OVERLOADED_THRESHOLD LBConf::almost_overloaded_threshold
#define SHARED_MLT_USED LBConf::shared_mlt_used

//starting stopping profiler, not really related to config itself but is handy to be placed here,
//since all classes have access to the this config class
#define prof_start() LBConf::loadbalancing_profiler->function_start(); 
#define prof_end() LBConf::loadbalancing_profiler->function_end(); 

class LBConf
{
	public:
		LBConf();
		
		static int init(const char* configfile);
		static int seconds_until_next_rebalance;
		static float cpu_threshold;
		static float w_swaps;
		static float w_threads;
		static float w_cpu;
		static int console_output;
		static int loglevel;
		static const char *logfile;
		static int max_no_of_threads;			
		static int max_no_of_swaps;
		static float thread_threshold;
		static float swaps_threshold;
		static int cpu_load_timespan;
		static int load_balancer_enabled;
		static int automatic_load_balancer_enabled;
		static float almost_overloaded_threshold;	
		static int shared_mlt_used;	
		
		static Pc2fsProfiler *loadbalancing_profiler; 
};

#endif

