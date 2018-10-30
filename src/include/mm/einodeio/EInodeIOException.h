/*
 * EInodeIOException.h
 *
 *  Created on: 25.05.2011
 *      Author: sergerit
 */

#ifndef EINODEIOEXCEPTION_H
#define EINODEIOEXCEPTION_H
#define EINODEIOEXCEPTION_EXCEPTION_MSG_LEN 512

/**
 * Exception used by the EmbeddedInodeLookUp
 */
class EInodeIOException
{
public:
    EInodeIOException(const char *message);
    virtual ~EInodeIOException() {};
    char *get_message();
private:
    char message[EINODEIOEXCEPTION_EXCEPTION_MSG_LEN];
};

#endif // EINODEIOEXCEPTION_H
