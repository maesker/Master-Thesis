#ifndef CONFIGURATIONMANAGER_H_
#define CONFIGURATIONMANAGER_H_

/**
 * @brief Manages commandline arguments and persistent configuration files
 * @author Sebastian Moors <mauser@smoors.de>, Markus Mäsker <maesker@gmx.net>
 * @file ConfigurationManager.h
 */

#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/tokenizer.hpp>

#include "exceptions/ConfigurationManagerException.h"


namespace po = boost::program_options;

/** 
* @class ConfigurationManager
* 
* @brief This class manages configuration parameters from different sources.
*
* The ConfigurationManager class is used to manage configuration values from different sources
* It is able to parse commandline arguments and read values from a config file.
* The commandline arguments override the same-named values from config files.
* The config file uses a very basic "name=value" syntax.
* 
* 
* Usage:
* 1. Create CM instance
* 2. register options:
*    e.g. cm->register_option("logfile","path to logfile");
* 3. parse input: cm->parse();
* 4. get values: std::string cm->get_value("logfile")
*
* 
* Section handling:
* To use sections consider the following example
* example.conf:
* [sys]
* port=8081
* 
* port will be represented by the cm as: 'sys.port'
* This means it must be registered the following way...
*   cm->register_option("sys.port","port of the mds");
* and can be accessed...
*   cm->get_value("sys.port");
* 
*  $Header $
**/

class ConfigurationManager
{
public:
    ~ConfigurationManager();
    ConfigurationManager(int argc, char **argv, std::string filename);
    void outputUsage(const boost::program_options::options_description& desc);
    std::string get_value(std::string option);
    int register_option(std::string option, std::string description);
    int parse();
    int16_t verify();
    
private:
    bool configmanager_started;
    int __argc;
    char **__argv;
    po::options_description *desc;
    po::variables_map vm;
		std::string configFileLocation;
};

#endif
