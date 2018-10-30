#ifndef COMPONENTEXCEPTION_H_
#define COMPONENTEXCEPTION_H_

/** 
 * @file MDSException.h
 * @author Markus Mäsker <maesker@gmx.net>
 * @date 25.07.11
 * */

#include <customExceptions/AbstractException.h>

/**
 * @class ComponentException
 * @brief Exception raised by the MetadataServer
 */
class ComponentException: public AbstractException
{
  public:
    ComponentException(char const *msg):AbstractException(msg){};
};

/** 
 * @class DataServerException
 * @brief Exception raised by the DataServer
 * */
class DataServerException: public AbstractException
{
  public:
    DataServerException(char const *msg):AbstractException(msg){};
};


/** 
 * @class DataServerException
 * @brief Exception raised by the DataServer
 * */
class FileStorageException: public AbstractException
{
  public:
    FileStorageException(char const *msg):AbstractException(msg){};
};


#endif //COMPONENTEXCEPTION_H_
