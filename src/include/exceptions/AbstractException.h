#ifndef ABSTRACT_EXCEPTION_H_
#define ABSTRACT_EXCEPTION_H_

/** 
 * @file AbstractException.h
 * @author Markus Mäsker <maesker@gmx.net>
 * @date 25.07.11
 * @brief Abstract exception class
 * */

#include <exception>
#include <stdlib.h>
#include <string>

using namespace std;

/**
 * @class AbstractException
 */
class AbstractException: public exception
{
  public:
    AbstractException(char const *message);
    ~AbstractException() throw();
    const char* what() const throw();
    
  private:
    string message;
};

#endif //ABSTRACT_EXCEPTION_H_
