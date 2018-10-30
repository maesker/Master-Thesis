/**
 * @file LBLogger.cpp
 * @class LBLogger
 * @date 28 May 2011
 * @author Denis Dridger
 *
 *
 * @brief A slightly extended "Logger" for a more convenient use with LB submodules
 * Uses singleton to support global access to LB submodules 
 */

#include <cstddef>
#include <cassert>

#include "coco/loadbalancing/LBLogger.h"
#include "coco/loadbalancing/LBConf.h"

LBLogger* LBLogger::__instance = NULL;

LBLogger::LBLogger()
{
	// TODO Auto-generated constructor stub

}

LBLogger::~LBLogger()
{
	// TODO Auto-generated destructor stub
}

void LBLogger::create_instance()
{
	if ( __instance == NULL )
	{
		__instance = new LBLogger;
	}
}

LBLogger* LBLogger::get_instance()
{
		assert(__instance);
		return __instance;
}

void LBLogger::init()
{	
	this->set_console_output(LB_CONSOLE_OUTPUT);
	this->set_log_level(LB_LOGLEVEL);
	this->set_log_location(LB_LOGFILE);
}

