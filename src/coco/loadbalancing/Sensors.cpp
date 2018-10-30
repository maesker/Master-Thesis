/**
 * @file Sensors.cpp
 * @class Sensors
 * @date April 2011
 * @author Sebastian Moors, Denis Dridger
 *
 *
 * @brief This class determines load metrics necessary to decide 
 * whether a server is overloaded or not. These load metrics are:
 * number of Ganesha swaps, number of Ganesha threads and averaged cpu load.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "coco/loadbalancing/LBConf.h"
#include "coco/loadbalancing/Sensors.h"
#include "coco/loadbalancing/LBLogger.h"


/**
 * @brief Helper function to convert the output of a shell command to an int
 * @param[in] the command that returns a string that shall be converted to int eventually
 * @return the output of the specified command as integer
 * */
int get_int_from_command( std::string command )
{
	prof_start();

	//see http://stackoverflow.com/questions/839232/popen-in-c
   	FILE *fpipe;
	//printf("running %s \n",command);
   	char line[2048];

	if ( !(fpipe = (FILE*)popen(command.c_str(),"r")) )
	{
   		perror("Problems with pipe");
 	}

 	fgets( line, sizeof line, fpipe);
	char *endPtr;
	int integer = strtol(line, &endPtr, 10);
	pclose(fpipe);

	prof_end();

	return integer;
}



/**
 * @brief Returns the system-wide limit for threads specified by the user
 * @return the system-wide limit for threads specified by the user
 * */
int Sensors::get_max_threads_per_process()
{
	prof_start();
	prof_end();
	return MAX_THREADS;
}


/**
 * @brief Returns number of threads that currently exist in the Ganesha process
 * @return number of threads that currently exist in the Ganesha process
 * */
int Sensors::get_no_of_ganesha_threads()
{
	prof_start();

	int pid = Sensors::get_ganesha_pid();
	char buffer[1024];
	sprintf(buffer, "ps -elm | grep %d | grep -v grep |wc -l", pid);
	int number_of_threads = get_int_from_command( buffer );

	prof_end();

	return number_of_threads;
}



/**
 * @brief requests number of swaps from Ganesha process
 * @param[in] pointer to Loadbalancing CommunicationHandler, which will invoke a message send to Ganesha process 
 * @return number of swaps
 */
uint64_t Sensors::get_ganesha_swapping(LBCommunicationHandler *com_handler)
{
	prof_start();
	
	log("Requesting number of swapps from Ganesha...");

	//message to be sent to Ganesha
	int message = REQ_SWAPS;	

	//request reply from Ganesha
	RequestResult reply = com_handler->ganesha_send((void*) &message, sizeof(ganesha_request_types_t));		
	uint64_t *swaps = (uint64_t*) (reply.data);
	
	log2("Swaps: %li", *swaps);

	prof_end();
	
	return *swaps;

}


/**
 * @brief Returns process ID of Ganesha
 * @return process ID of Ganesha
 * */
int Sensors::get_ganesha_pid()
{
	prof_start();

	int pid = get_int_from_command( "ps aux | grep 'zmq.ganesha.nfsd' | head -n 1 | cut -f 2 -d ' '" );	

	prof_end();

	return pid;	
}


/**
@brief returns averaged CPU load measured within CPU_LOAD_TIMESPAN where this value can be 1, 5 or 15 (minutes)
note: the average load value is calculated by the kernel and is not a value between 0 and 1 but a value between 0 and > 1
see http://blog.scoutapp.com/articles/2009/07/31/understanding-load-averages 
and http://www.linuxjournal.com/article/9001 to understand what average load is  
@return averaged CPU load
*/
float Sensors::get_average_load()
{
	prof_start();

	FILE *afile;
	afile = fopen("/proc/loadavg", "r");
	
	float load;

	char *line = new char[128]; 
	char *result = new char[8];	
		
	if (!afile == 0) fgets(line, 128, afile);	

  	char *p;  
  	p = strtok(line," ");
  		
	int k = -1;
	if (CPU_LOAD_TIMESPAN == 1)	k = 0;	
	if (CPU_LOAD_TIMESPAN == 5) k = 1;		
	if (CPU_LOAD_TIMESPAN == 15) k = 2;
	if (k == -1) k = 1;

	int j = 0;			
  	while (p != NULL)
  	{
		if (j == k) memcpy(result, p, 8);
    		//printf ("%s\n", p);
    		p = strtok (NULL, " ,");
		j++;
  	}

	fclose(afile);						
	
	//str to float	
	load = atof(result);		

	delete [] line;		
	delete [] result;															
	
	prof_end();

	return load;

}

/**
 * @brief returns number of installed CPU cores
 * Number of CPU cores is needed to determine the actual workload of the CPU
 * @return number of CPU cores
 * */
int Sensors::get_number_of_cpu_cores()
{
	prof_start();

	int ret = get_int_from_command("cat /proc/cpuinfo | grep cores | head -n 1 | cut -f 3 -d ' '");	

	prof_end();
	
	//if ret has no feasible value then set it to 1 
	//(not feasible value can only occur for machines with 1 core, those aren't used nowadays anyway)	
	if ((ret < 1) || (ret > 128) ) return 1; 
	else return ret;
}


/**
 * @brief returns true if the CPU is considered to be overloaded
 * cpu is overloaded <=> all cores are overloaded
 * meaning: if there are x cpu cores and average load is y > x then the cpu is overloaded 
 * example: 
 *	one-core cpu and average load is < 1.0 => ok 
 *	one-core cpu and average load is > 1.0 => cpu is overloaded 
 *	two-core cpu and average load is < 2.0 => ok 
 *	two-core cpu and average load is > 2.0 => cpu is overloaded
 * factor CPU_THRESHOLD is by default 1. Setting it to < 1 considers a cpu as overloaded even if it still can take some load
 * @return true if CPU is overloaded
 * */

bool Sensors::cpu_overloaded()
{	
	prof_start();

	if ( Sensors::get_average_load() > (Sensors::get_number_of_cpu_cores() * CPU_THRESHOLD) )
	{
		prof_end();

		return true;
	}
	else 
	{
		prof_end();
		return false;	
	}
}



/**
 * @brief decides whether thread limit for the ganesha process is reached
 * @return true if specified thread limit is reached
 * */
bool Sensors::thread_limit_reached()
{
	prof_start();	
	//system is considered to be overloaded if there are more active threads than max (possible threads * THREAD_THRESHOLD)
	//where THREAD_THRESHOLD is between 0 and 1  	
	if ( get_no_of_ganesha_threads() > (MAX_THREADS * THREAD_THRESHOLD) )
	{
		prof_end();
		return true;
	}
	else
	{
		prof_end();
		return false;
	}
}

/**
 * @brief decides whether swap limit is reached. i.e. there are so many ganesha swaps that a rebalancing procedure is required
 * @return true if specified swap limit is reached
 * */
bool Sensors::swap_limit_reached(LBCommunicationHandler *com_handler)
{
	prof_end();
	//system is considered to be overloaded if there are ganesha swaps than (max allowed swaps * SWAP_THRESHOLD)
	//where SWAPS_THRESHOLD is between 0 and 1  	
	if ( get_ganesha_swapping(com_handler) > (MAX_SWAPS * SWAPS_THRESHOLD) )
	{
		prof_end();
		return true;
	}
	else
	{
		prof_end();
		return false;
	}
}

/**
 * @brief returns normalized load of a server, specified by ServerLoad struct
 * That is, load metrics "cpu load", "no of threads" and "no of swaps" is dumped into a single value
 * considering users weightings concerning the importance of each load metric. 
 * @return normalized server load 
 */
float Sensors::normalize_load(float cpu_load, int swaps, int threads)
{
	prof_start();
	
	//explicit int -> float
	float cpu_cores = Sensors::get_number_of_cpu_cores();	
	float max_swaps = MAX_SWAPS;
	float max_threads = MAX_THREADS;	

	float normalized_cpu_load = ((cpu_load / cpu_cores) * W_CPU);		

	float normalized_swaps = ((swaps / max_swaps) * W_SWAPS);		

	float normalized_threads = ((threads / max_threads) * W_THREADS);
	
	prof_end();
	
	return (normalized_cpu_load + normalized_swaps + normalized_threads);
}





