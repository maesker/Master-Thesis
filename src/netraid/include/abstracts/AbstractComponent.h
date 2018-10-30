#ifndef ABSTRACT_MDS_COMPONENT_H_
#define ABSTRACT_MDS_COMPONENT_H_

/** 
 * @file  AbstractComponent.h
 * @class AbstractComponent
 * @author Markus Mäsker, maesker@gmx.net
 * @date  25.07.11
 * 
 * @brief All MDS components descent from this Class and inherit its methods.
 * 
 * 
 * This component instanziated a Logger instance, a ConfigurationManager 
 * instance, the Pc2fsProfiler instance and provides some basic function 
 * that can be extended or replaced 
 * by the inheriting component.
 * 
 * */

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

#include "components/configurationManager/ConfigurationManager.h"
#include "global_types.h"

#include "logging/Logger.h"

#include "pc2fsProfiler/Pc2fsProfiler.h"

//#include "mm/mds/mds_sys_tools.h"

using namespace std;


/** 
 * @enum Status of the Components
 * */
enum ComponentStatus_t
{
    uninitialized,
    initialized,
    started,
    stopped            
};


/**
 * @class AbstractComponent
 */
class AbstractComponent
{          
    private:
        // methods

    protected:  
        // components
        ConfigurationManager *p_cm;
        Pc2fsProfiler *p_profiler;

        
        // variables
        struct mallinfo mi_init_start;
        struct mallinfo mi_init_end;
        struct mallinfo mi_temp_start;
        struct mallinfo mi_temp_end;    
        ComponentStatus_t status;
        std::string basedir;
        std::string absolute_conffile;
        
                        
        // methods
        void init_default_cm();
        void init_logger();
        void detect_memoryleak_start();
        int32_t detect_memoryleak_end();
        void set_status(ComponentStatus_t s);

    public:
        // constructor, destructor
        explicit AbstractComponent(std::string basedir, std::string conffile);
        ~AbstractComponent() throw();
        
        // components
        Logger *log;
        
        // methods
        virtual int32_t run();
        virtual int32_t stop();
        virtual int16_t verify();
        ComponentStatus_t get_status();
        

};

#endif //ABSTRACT_MDS_COMPONENT_H_
