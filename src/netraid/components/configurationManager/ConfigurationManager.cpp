/**
 * @file ConfigurationManager.cpp
 * @class ConfigurationManager
 * @brief Manages commandline arguments and persistent configuration files
 * @author Sebastian Moors <mauser@smoors.de>, Markus Mäsker maesker@gmx.net
 */

#include <iostream>
#include "components/configurationManager/ConfigurationManager.h"

namespace po = boost::program_options;

/**
* @brief Display informations about the possible commandline arguments
* @param desc The possible arguments.
* */
void ConfigurationManager::outputUsage(const po::options_description& desc)
{
    std::cout << desc;
}

/**
* @brief Constructor for the ConfigurationManager class
* @param argc The number of commandline arguments
* @param argc Pointers to the commandline arguments
* @param file The absolute location of the configuration file which should be used
* */
ConfigurationManager::ConfigurationManager(int argc, char **argv, std::string file)
{
    
    boost::filesystem::path p(file);
    if(is_regular_file(p)) {
        configFileLocation = file;
    }
    else
    {
        configFileLocation = "";
        std::cout << "No file found:" << file << std::endl;
    }
    __argc=argc;
    __argv=argv;
    desc =  new po::options_description("Allowed options");
    this->desc = desc;
    desc->add_options()("help,h,?", "Display this message");
}

/**
 * @brief Registers a new option. Simple option=value interface. Option
 * musst not equal 'help', 'h' or '?'.
 * @param[in] option String representing the option to register
 * @param[in] description String describing the option
 * @return 0 if ok, -1 on error
 * */
int ConfigurationManager::register_option(std::string option, std::string description)
{
    if (this->desc != NULL)
    {
        switch ((!option.compare("help") || !option.compare("h") || !option.compare("?")))
        {
        case 1:
            return -1;
        default:
            this->desc->add_options()(option.c_str(),po::value<std::string>(),description.c_str());
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Parse the commandline arguments and if provided, the config
 * file.
 * */
int ConfigurationManager::parse()
{
    try
    {
        po::store(po::parse_command_line(__argc, __argv, *this->desc), vm);
    }
    catch(...)
    {
        std::cout << "An error happened while parsing the command line options" << std::endl;
        return -1;
    }
    po::notify(vm);
    if (__argc > 1 && (!strcmp(__argv[1], "-?") || !strcmp(__argv[1], "--?") || !strcmp(__argv[1], "/?") || !strcmp(__argv[1], "/h") || !strcmp(__argv[1], "-h") || !strcmp(__argv[1], "--h") || !strcmp(__argv[1], "--help") || !strcmp(__argv[1], "/help") || !strcmp(__argv[1], "-help") || !strcmp(__argv[1], "help") ))
    {
        outputUsage(*this->desc);
        return 0;
    }
    try
    {
        if (configFileLocation.length() > 0)
        {
            //std::cout << "XXX" << configFileLocation << std::endl;
            boost::filesystem::ifstream configFile;
            configFile.open( boost::filesystem::path( configFileLocation ));
            std::stringstream ss;
            std::string str(ss.str());
            ss << configFile.rdbuf();
            po::store( po::parse_config_file(ss, *this->desc,true), vm);
            po::notify(vm);
        }
    }
    catch (...)
    {
        std::cout << "An error happenend while parsing the configuration file." << std::endl;
        return -1;
    }
    configmanager_started=1;
    return 0;
}

/**
 * @brief look for the provided option and return the value, if
 * provided.
 * */
std::string ConfigurationManager::get_value(std::string option)
{
    if (!configmanager_started)
    {
        parse();
    }
    std::string s = std::string();
    for(po::variables_map::iterator it = vm.begin(); it != vm.end(); ++it)
    {
        if (!option.compare(it->first))
        {
            s.assign((std::string) it->second.as<std::string>());
            break;
        }
    }
    return s;
}

int16_t ConfigurationManager::verify()
{
    if (configmanager_started)
    {
        return 1;
    }
    return 0;
}
