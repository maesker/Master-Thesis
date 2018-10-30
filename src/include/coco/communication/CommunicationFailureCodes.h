/**
 * \file CommunicationFailureCodes.h
 *
 * \author Marie-Christine Jakobs
 *
 * \brief Defines the failure codes returned by a failure during communication
 *
 *
 */
#ifndef _COMMUNICATIONFAILURECODES_H_
#define _COMMUNICATIONFAILURECODES_H_

/**
 * Enumeration of the used failures codes which are returned if anything fails during the usage of a communication component. Notice that 0 is
 * reserved to indicate a correct execution.
 */
enum CommunicationFailureCodes {MutexFailed=1, UnableToCreateMessage=2, SocketNotExisting=3, SendingFailed=4, UnknownMessageId = 5, Timeout = 6, NotEnoughMemory = 7, SocketAlreadyExisting = 8 , SocketCreationFailed = 9, WrongMltData = 10, SocketDecreationFailed = 11};

#endif //_COMMUNICATIONFAILURECODES_H_
