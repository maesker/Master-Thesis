#ifndef LOCKMANAGER_H
#define LOCKMANAGER_H

#include <unordered_map>
#include <string>
#include <pthread.h>
#include "global_types.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

#define SLEEP_TIME 10000000

typedef struct {
	int waiting_threads;
	pthread_mutex_t lock;
	pthread_cond_t condition;
} ObjectLock;

class LockManager
{
public:

    LockManager();
    virtual ~LockManager();
    void lock_object(const char *identifier);
    void unlock_object(const char *identifier);
    bool is_locked(const char *identifier);

private:
    std::unordered_map<std::string, ObjectLock*> *locked_objects;
    pthread_mutex_t lock;
    Pc2fsProfiler *profiler;
};

#endif // LOCKMANAGER_H
