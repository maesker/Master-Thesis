#ifndef CONFMANEXCEPTION_H_
#define CONFMANEXCEPTION_H_

/** 
 * @file ConfigurationManagerException.h
 * @author Markus Mäsker <maesker@gmx.net>
 * @date 27.07.11
 * @class ConfManException
 * @brief Implements the ConfigurationManager exception
 * */

#include <customExceptions/AbstractException.h>

class ConfManException: public AbstractException
{
  public:
    ConfManException(char const *msg):AbstractException(msg){};
};

#endif
