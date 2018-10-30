#ifndef ABSTRACTCOMPEXCEPTION_H_
#define ABSTRACTCOMPEXCEPTION_H_

/** 
 * @file AbstractMdsComponentException.h
 * @author  Markus Mäsker <maesker@gmx.net>
 * @date 25.07.11
 * @class AbstractMdsComponentException
 * @brief Implements the basic MDS Components exception
 * */

#include "customExceptions/AbstractException.h"

class AbstractComponentException: public AbstractException
{
  public:
    AbstractComponentException(char const *msg):AbstractException(msg){};
};

#endif
