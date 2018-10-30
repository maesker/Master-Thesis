#ifndef MDSEXCEPTION_H_
#define MDSEXCEPTION_H_

/** 
 * @file MDSException.h
 * @author Markus Mäsker <maesker@gmx.net>
 * @date 25.07.11
 * */

#include <exceptions/AbstractException.h>

/**
 * @class MDSException
 * @brief Exception raised by the MetadataServer
 */
class MDSException: public AbstractException
{
  public:
    MDSException(char const *msg):AbstractException(msg){};
};

/** 
 * @class SubtreeManagerException
 * @brief Exception raised by the SubtreeManager
 * */
class SubtreeManagerException: public AbstractException
{
  public:
    SubtreeManagerException(char const *msg):AbstractException(msg){};
};


/** 
 * @class MRException
 * @brief Exception raised by the MessageRouter
 * */
class MRException: public AbstractException
{
  public:
    MRException(char const *msg):AbstractException(msg){};
};

/** 
 * @class MJIException
 * @brief Exception raised by the mds_journal_interface_lib.h functions
 * */
class MJIException: public AbstractException
{
  public:
    MJIException(char const *msg):AbstractException(msg){};
};

/**
 * @class ZMQMessageException
 * @brief Exception raised by the worker thread if zmq_recv returns non zero.
 */
class ZMQMessageException: public AbstractException
{
  public:
    ZMQMessageException(char const *msg):AbstractException(msg){};
};


#endif //MDSEXCEPTION_H_
