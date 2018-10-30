#include "exceptions/AbstractException.h"

/** 
 * @class AbstractException
 * @author Markus Maesker <maesker@gmx.net>
 * @date 25.07.11
 * @brief Main Exception class. 
 * @file AbstractException.cpp
 * 
 * Inherits the exception module and implements the what function
 * */

/** 
 * @brief Constructor
 * @param[in] message const char pointer representing the exception message
 * */
AbstractException::AbstractException(char const *message)
{
    this->message = string(message);
}

/** 
 * @brief Descructor
 * */
AbstractException::~AbstractException() throw()
{
}

/** 
 * @brief Implements the exception.what function. 
 * @return Returns the message as a const char pointer.
 * */
const char* AbstractException::what() const throw()
{
    return this->message.c_str();
}
