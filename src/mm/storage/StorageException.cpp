#include "mm/storage/StorageException.h"
#include <stdio.h>

StorageException::StorageException(const char *message)
{
    snprintf(this->message, STORAGE_EXCEPTION_MSG_LEN, message);
}

char *StorageException::get_message()
{
    return this->message;
}
