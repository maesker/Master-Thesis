#ifndef MDSEXCEPTION_H_
#define MDSEXCEPTION_H_

/** 
 * @file AbstractMdsComponentException.h
 * @author  Markus Mäsker <maesker@gmx.net>
 * @date 25.07.11
 * @class AbstractMdsComponentException
 * @brief Implements the basic MDS Components exception
 * */

#include <exceptions/AbstractException.h>

class AbstractMdsComponentException: public AbstractException
{
  public:
    AbstractMdsComponentException(char const *msg):AbstractException(msg){};
};

#endif
