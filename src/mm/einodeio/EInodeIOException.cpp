/*
 * EInodeIOException.cpp
 *
 *  Created on: 25.05.2011
 *      Author: sergerit
 */

#include <string.h>

#include "mm/einodeio/EInodeIOException.h"

EInodeIOException::EInodeIOException(const char *message)
{
    strncpy(this->message, message, EINODEIOEXCEPTION_EXCEPTION_MSG_LEN);
}

char *EInodeIOException::get_message()
{
    return this->message;
}
