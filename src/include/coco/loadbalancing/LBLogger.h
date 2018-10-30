/**
 * @file LBLogger.h
 * 
 * @date 28 May 2011
 * @author Denis Dridger
 *
 *
 * @brief A slightly extended "Logger" for a more convenient use with LB submodules
 * Uses singleton to support global access to LB submodules 
 */

#ifndef LBLOGGER_H_
#define LBLOGGER_H_

//some makros to avoid writing "LBLogger::get_instance()->debug_log(...)" all the time
#define log(par1) LBLogger::get_instance()->debug_log(par1);
#define log2(par1,par2) LBLogger::get_instance()->debug_log(par1,par2);
#define log3(par1,par2,par3) LBLogger::get_instance()->debug_log(par1,par2,par3);
#define log4(par1,par2,par3,par4) LBLogger::get_instance()->debug_log(par1,par2,par3,par4);

//#include "../../logging/Logger.h" //cant use this, conflict with communication module
#include "../communication/CommunicationLogger.h"

class LBLogger: public Logger
{
public:
	LBLogger();
	virtual ~LBLogger();

	static void create_instance();

	static LBLogger* get_instance();

	void init();

	private:
	static LBLogger* __instance;

};

#endif /* LBLOGGER_H_ */



