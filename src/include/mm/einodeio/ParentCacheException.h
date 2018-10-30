#ifndef PARENTCACHEEXCEPTION_H
#define PARENTCACHEEXCEPTION_H

#define MSG_LEN 256

class ParentCacheException
{
    public:
        ParentCacheException(char *msg);
        virtual ~ParentCacheException() {};
        char *get_msg();
    private:
        char msg[MSG_LEN];
};

#endif // PARENTCACHEEXCEPTION_H
