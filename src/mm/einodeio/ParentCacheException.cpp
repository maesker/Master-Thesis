#include <string.h>
#include "mm/einodeio/ParentCacheException.h"

ParentCacheException::ParentCacheException(char *msg)
{
    strncpy(this->msg, msg, MSG_LEN);
}

char *ParentCacheException::get_msg()
{
    return this->msg;
}
