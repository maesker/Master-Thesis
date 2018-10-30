#ifndef STORAGEEXCEPTION_H
#define STORAGEEXCEPTION_H

#define STORAGE_EXCEPTION_MSG_LEN 512
/**
 * Exception used by the StorageAbstractionLayer
 */
class StorageException
{
public:
    StorageException(const char *message);
    virtual ~StorageException() {};
    char *get_message();
private:
    char message[STORAGE_EXCEPTION_MSG_LEN];
};

#endif // STORAGEEXCEPTION_H
