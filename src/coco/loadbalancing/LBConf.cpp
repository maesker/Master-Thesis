/**
 * @file LBConf.cpp
 * @class LBConf
 * @date 20 July 2011
 * @author Denis Dridger
 *
 *
 * @brief This class reads configuration values from a file 
* and provides them to the loadbalancing managers.
 */


#include "coco/loadbalancing/LBConf.h"
#include "coco/loadbalancing/LBLogger.h"
#include <stdexcept>
#include <vector>
#include <stdio.h>
#include <fstream>
#include <stdio.h>
#include <string.h>


int LBConf::seconds_until_next_rebalance = -1;
float LBConf::cpu_threshold  = -1;
float LBConf::thread_threshold = -1;
float LBConf::swaps_threshold = -1;
float LBConf::w_swaps  = -1;
float LBConf::w_threads  = -1;
float LBConf::w_cpu  = -1;
int LBConf::console_output  = -1;
int LBConf::loglevel  = -1;
const char *LBConf::logfile  = NULL;
int LBConf::max_no_of_threads  = -1;
int LBConf::max_no_of_swaps  = -1;
int LBConf::cpu_load_timespan  = -1;
int LBConf::load_balancer_enabled = -1;
float LBConf::almost_overloaded_threshold = -1;
Pc2fsProfiler *LBConf::loadbalancing_profiler = NULL;
int LBConf::automatic_load_balancer_enabled = -1;
int LBConf::shared_mlt_used = -1;

/**
 * @brief just gets pointer to profiler, which then can be accessed from any load balancing class
 */
LBConf::LBConf()
{
	loadbalancing_profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief Read values from loadbalancing config file and assign them to static variables
 * @param configfile[in] path to loadbalancing configuration file
 * @return 0 on success 
 */
 int LBConf::init(const char* configfile)
{
	ifstream input;
    
	vector<string> lines;
    
	string line;

	string temp;

	//open file, if file could not be opened an exception is thrown and not catched
	input.open(configfile);
    
	if (input.fail())
    	{
        	try
        	{
            		input.close();
        	}
        	catch (exception e)
        	{
            		//do nothing
        	}

		log("Error opening loadbalancing configuration file. Aborting...");

        	throw runtime_error("Loadbalancing configuration file cannot be opened.");
	}  
	
	string s1("seconds_until_next_rebalance");
	string s2("cpu_threshold");
	string s3("w_swaps");
	string s4("w_threads");
	string s5("w_cpu");
	string s6("console_output");
	string s7("loglevel");
	string s8("logfile");
	string s9("max_no_of_threads");		
	string s10("thread_threshold");		
	string s11("max_no_of_swaps");
	string s12("swaps_threshold");
	string s13("cpu_load_timespan");
	string s14("load_balancer_enabled");
	string s15("almost_overloaded_threshold");
	string s16("automatic_load_balancer_enabled");
	string s17("shared_mlt_used");

	//parse config file line by line and set the corresponding variables
  	while(getline(input, line))
	{
		//parse seconds_until_next_rebalance settings
		if (line.compare(0, s1.length(), s1) == 0)
		{			
			seconds_until_next_rebalance = atoi( line.substr(s1.length()+1,line.length()).c_str() );							
		}

		//parse cpu_threshold settings
		if (line.compare(0, s2.length(), s2) == 0)
		{			
			cpu_threshold = atof( line.substr(s2.length()+1,line.length()).c_str() );							
		}

		//parse w_swaps settings
		if (line.compare(0, s3.length(), s3) == 0)
		{			
			w_swaps = atof( line.substr(s3.length()+1,line.length()).c_str() );							
		}

		//parse w_threads settings
		if (line.compare(0, s4.length(), s4) == 0)
		{			
			w_threads = atof( line.substr(s4.length()+1,line.length()).c_str() );							
		}

		//parse w_cpu settings
		if (line.compare(0, s5.length(), s5) == 0)
		{			
			w_cpu = atof( line.substr(s5.length()+1,line.length()).c_str() );							
		}

		//parse console_output settings
		if (line.compare(0, s6.length(), s6) == 0)
		{			
			console_output = atoi( line.substr(s6.length()+1,line.length()).c_str() );							
		}

		//parse loglevel settings
		if (line.compare(0, s7.length(), s7) == 0)
		{			
			loglevel = atoi( line.substr(s7.length()+1,line.length()).c_str() );							
		}

		//parse logfile settings
		if (line.compare(0, s8.length(), s8) == 0)
		{			
			int size = line.substr(s8.length()+1,line.length()).size();
			logfile = (char*) malloc (size+1);		
			memcpy((char*)logfile, line.substr(s8.length()+1,line.length()).c_str(), size+1);									
		}

		//parse max_no_of_threads settings
		if (line.compare(0, s9.length(), s9) == 0)
		{			
			max_no_of_threads = atoi( line.substr(s9.length()+1,line.length()).c_str() );						
		}

		//parse thread_limit settings
		if (line.compare(0, s10.length(), s10) == 0)
		{			
			thread_threshold = atof( line.substr(s10.length()+1,line.length()).c_str() );							
		}

		//parse max_no_of_swaps settings
		if (line.compare(0, s11.length(), s11) == 0)
		{			
			max_no_of_swaps = atof( line.substr(s11.length()+1,line.length()).c_str() );							
		}
	
		//parse swaps_threshold settings
		if (line.compare(0, s12.length(), s12) == 0)
		{			
			swaps_threshold = atof( line.substr(s12.length()+1,line.length()).c_str() );							
		}

		//parse cpu_load_timespan settings
		if (line.compare(0, s13.length(), s13) == 0)
		{			
			cpu_load_timespan = atoi( line.substr(s13.length()+1,line.length()).c_str() );							
		}

		//parse load_balancer_enabled settings
		if (line.compare(0, s14.length(), s14) == 0)
		{			
			load_balancer_enabled = atoi( line.substr(s14.length()+1,line.length()).c_str() );							
		}

		//parse almost_overloaded_threshold settings
		if (line.compare(0, s15.length(), s15) == 0)
		{			
			almost_overloaded_threshold = atof( line.substr(s15.length()+1,line.length()).c_str() );							
		}

		//parse automatic_load_balancer_enabled settings
		if (line.compare(0, s16.length(), s16) == 0)
		{			
			automatic_load_balancer_enabled = atoi( line.substr(s16.length()+1,line.length()).c_str() );							
		}

		//parse shared_mlt_used settings
		if (line.compare(0, s17.length(), s17) == 0)
		{			
			shared_mlt_used = atoi( line.substr(s17.length()+1,line.length()).c_str() );							
		}


	}    

	//check whether all values could be set properly, abort execution if not

	if ( (seconds_until_next_rebalance == -1) ) 
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'seconds_until_next_rebalance' has bad entry or doesn't exist. Aborting...");
	
	if ( (cpu_threshold == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'cpu_threshold' has bad entry or doesn't exist. Aborting...");
	
	if ( (w_swaps == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'w_swaps' has bad entry or doesn't exist. Aborting...");
	
	if ( (w_threads == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'w_threads' has bad entry or doesn't exist. Aborting...");
	
	if ( (w_cpu == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'w_cpu' has bad entry or doesn't exist. Aborting...");
		
	if ( (console_output == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'console_output' has bad entry or doesn't exist. Aborting...");
		
	if ( (loglevel == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'loglevel' has bad entry or doesn't exist. Aborting...");
		
	if ( (logfile == NULL) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'logfile' has bad entry or doesn't exist. Aborting...");
		
	if ( (max_no_of_threads == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'max_no_of_threads' has bad entry or doesn't exist. Aborting...");
	
	if ( (thread_threshold == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'thread_threshold' has bad entry or doesn't exist. Aborting...");

	if ( (swaps_threshold == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'swaps_threshold' has bad entry or doesn't exist. Aborting...");

	if ( (max_no_of_swaps == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'max_no_of_swaps' has bad entry or doesn't exist. Aborting...");
	
	if ( (cpu_load_timespan == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'cpu_load_timespan' has bad entry or doesn't exist. Aborting...");
	
	if ( (load_balancer_enabled == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'load_balancer_enabled' has bad entry or doesn't exist. Aborting...");
	
	if ( (almost_overloaded_threshold == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'almost_overloaded_threshold' has bad entry or doesn't exist. Aborting...");

	if ( (automatic_load_balancer_enabled == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'automatic_load_balancer_enabled' has bad entry or doesn't exist. Aborting...");

	if ( (shared_mlt_used == -1) )
		throw runtime_error("Error while parsing loadbalancing configuration file: Entry 'shared_mlt_used' has bad entry or doesn't exist. Aborting...");
			



    	try
    	{
        	input.close();
        	input.clear();
    	}
    	catch (exception e)
    	{
        	//do nothing
    	}

	//not needed actually 
	return 0; 
}


