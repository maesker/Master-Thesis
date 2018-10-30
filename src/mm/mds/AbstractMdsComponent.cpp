/** 
 * @file AbstractMdsComponent.cpp
 * @author Markus Maesker, maesker@gmx.net
 * @date 25.07.11
 * */

#include "mm/mds/AbstractMdsComponent.h"

/** 
 * @class AbstractMdsComponent
 * @brief The base Component that all MDS modules inherit.
 * @param[in] conffile String representing the config files path.
 * */
AbstractMdsComponent::AbstractMdsComponent(std::string basedir, std::string conffile)
{
    // initialize memory allocation status
    mi_temp_start = mallinfo();
    p_profiler = Pc2fsProfiler::get_instance();
    this->basedir = basedir;
    
    set_status(uninitialized);   
    absolute_conffile = basedir;
    if (*(basedir.rbegin()) != '/' )
    {
        absolute_conffile+="/";
    }
    absolute_conffile+=conffile;
    // initialize ConfigurationManager
    init_default_cm();
    
    // initialize Logger
    init_logger();
}

/** 
 * @brief Delete the Component
 * */
AbstractMdsComponent::~AbstractMdsComponent() throw()
{
    delete p_cm;
    delete log;
}

/**
 * @brief Initialize ConfigManager
 * @param[in] conffile Path to the configfile
 * */
void AbstractMdsComponent::init_default_cm()
{
    try
    {
        // create Configuration Manager
        p_cm = new ConfigurationManager(0, NULL,absolute_conffile);
        // add default options
        p_cm->register_option( "logfile", "Path to logfile");
        p_cm->register_option( "loglevel", "Specify loglevel");
        p_cm->register_option( "cmdoutput", "Activate commandline output");
    }
    catch(ConfManException e)
    {
        if (p_cm != NULL)
        {
            delete p_cm;
        }
        std::string s = std::string( "ConfigurationManager: %s.", e.what() );
        throw AbstractMdsComponentException( s.c_str() );
    }
} // end of init_default_cm

/** 
 * @brief Initialize the Logger instance
 * */
void AbstractMdsComponent::init_logger()
{
    // initialize logger with options read from config file
    log = new Logger();
    if (p_cm != NULL)
    {
        int loglevel = atoi( p_cm->get_value( "loglevel" ).c_str() );
        int cmdoutput = atoi( p_cm->get_value( "cmdoutput" ).c_str() );
        std::string logfile = p_cm->get_value( "logfile" );
        if (logfile.length()<1)
        {
            logfile = "default.log";
        }
        log->set_log_level( loglevel );
        log->set_console_output( (bool)cmdoutput );
        std::string log_location = std::string(DEFAULT_LOG_DIR) + "/" + logfile; 
        //std::string log_location = basedir + "/" + logfile; 
        log->set_log_location( log_location );
        log->debug_log( "Logging to location: %s" , log_location.c_str() );
    }
    else
    {
        throw ConfManException("Config manager not created.");
    }
}

/** 
 * @brief  Startup the components. Usually this method is overridden by the 
 * inheriting class
 * */
int32_t AbstractMdsComponent::run()
{
    set_status(started);
}

/** 
 * @brief  Stop the components. Usually this method is overridden by the 
 * inheriting class
 * */
int32_t AbstractMdsComponent::stop()
{
    set_status(stopped);
}

/** 
 * @brief Detects the memory usage and compares it with the 
 * value stored during the detect_memoryleak_start method
 * @return diff of the memory usage between the start-end checks.
 * */
int32_t AbstractMdsComponent::detect_memoryleak_end()
{
    mi_temp_end = mallinfo();
    int32_t diff = mi_temp_end.uordblks-mi_temp_start.uordblks;
    //if(diff != 0)
    //{
        log->debug_log("Diff:%u.\n",diff);
    //}
    return diff;
}

/** 
 * @brief Detect the memory usage to compare it when detect_memoryleak_end 
 * is issued
 * */
void AbstractMdsComponent::detect_memoryleak_start()
{
    mi_temp_start = mallinfo();
}

/** 
 * @brief sets the status
 * @param[in] st status to set
 * */
void AbstractMdsComponent::set_status(ComponentStatus_t st)
{
    status = st;
}

/** 
 * @brief return status
 * @return status
 * */
ComponentStatus_t AbstractMdsComponent::get_status()
{
    return status;
}

/** 
 * @brief Verify that all used components work
 * @return 0 if a component fails, 1 if successful
 * */
int16_t AbstractMdsComponent::verify()
{
    if (!log->verify())
    {
        return 0;
    }
    if (!p_cm->verify())
    {
        return 0;
    }
    if (get_status()!=started)
    {
        return 0;
    }
    return 1;
}
