/**
 * @file Sensors.h
 *
 * @date April 2011
 * @author Sebastian Moors, Denis Dridger
 *
 *
 * @brief This class determines load metrics necessary to decide 
 * whether a server is overloaded or not. These load metrics are:
 * number of Ganesha swaps, number of Ganesha threads and averaged cpu load.
 */

#include "LBCommunicationHandler.h"

class Sensors
{
	public:
		static int get_no_of_ganesha_threads();
		static uint64_t get_ganesha_swapping(LBCommunicationHandler *com_handler);
		static float get_average_load();

		//rebalancing indicators
		static bool cpu_overloaded();
		static bool thread_limit_reached();
		static bool swap_limit_reached(LBCommunicationHandler *com_handler);
		static int get_number_of_cpu_cores();	
		static float normalize_load(float cpu_load, int swaps, int threads);

	private:
		static int get_max_threads_per_process();
		static int get_ganesha_pid();
			
};
