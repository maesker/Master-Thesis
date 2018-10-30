#include "mm/storage/LockManager.h"
#include <string.h>
#include <unistd.h>


/**
 * @brief Constructor for LockManager
 */
LockManager::LockManager()
{
    this->locked_objects = new std::unordered_map<std::string, ObjectLock*>();
    this->lock = PTHREAD_MUTEX_INITIALIZER;
    this->profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief Destructor for LockManager
 */
LockManager::~LockManager()
{
    delete this->locked_objects;
}

/**
 * @brief Lock metadata object
 *
 * Blocks until object is unlocked
 *
 * @param[in] identifier Object to lock
 */
void LockManager::lock_object(const char* identifier)
{
	profiler->function_start();
	std::string obj_str = std::string(identifier);
	profiler->function_sleep();
	pthread_mutex_lock(&(this->lock));
	profiler->function_wakeup();
	std::unordered_map<std::string, ObjectLock*>::const_iterator it;
	it =  this->locked_objects->find(obj_str);
	if(it != this->locked_objects->end())
	{
		ObjectLock *object_lock = it->second;
		profiler->function_sleep();
		pthread_mutex_lock(&(object_lock->lock));
		profiler->function_wakeup();
		object_lock->waiting_threads++;
		pthread_mutex_unlock(&(this->lock));
		profiler->function_sleep();
		pthread_cond_wait(&(object_lock->condition), &(object_lock->lock));
		profiler->function_wakeup();
		pthread_mutex_unlock(&(object_lock->lock));
	}
	else
	{
		ObjectLock *object_lock = new ObjectLock;
		object_lock->lock = PTHREAD_MUTEX_INITIALIZER;
		pthread_cond_init(&(object_lock->condition),NULL);
		object_lock->waiting_threads = 0;
		this->locked_objects->insert(std::unordered_map<std::string, ObjectLock*>::value_type(obj_str, object_lock));
		pthread_mutex_unlock(&(this->lock));
	}
	profiler->function_end();
}

/**
 * @brief Unlock metadata object
 * @param[in] identifier Object to unlock_object
 */
void LockManager::unlock_object(const char* identifier)
{
	profiler->function_start();
	std::string obj_str = std::string(identifier);
	profiler->function_sleep();
	pthread_mutex_lock(&(this->lock));
	profiler->function_wakeup();

	std::unordered_map<std::string, ObjectLock*>::const_iterator it;
	it = this->locked_objects->find(obj_str);

	if(it ==  this->locked_objects->end())
	{
		pthread_mutex_unlock(&(this->lock));
		profiler->function_end();
		return;
	}

	ObjectLock *object_lock = it->second;

	profiler->function_sleep();
	pthread_mutex_lock(&(object_lock->lock));
	profiler->function_wakeup();

	if(object_lock->waiting_threads == 0)
	{
		pthread_mutex_unlock(&(object_lock->lock));
		pthread_cond_destroy(&(object_lock->condition));
		pthread_mutex_destroy(&(object_lock->lock));
		delete object_lock;
		this->locked_objects->erase(it);
	}
	else
	{
		object_lock->waiting_threads--;
		pthread_cond_signal(&(object_lock->condition));
		pthread_mutex_unlock(&(object_lock->lock));
	}

    pthread_mutex_unlock(&(this->lock));
    profiler->function_end();
}

/**
 * @brief Check if object is locked
 * @param[in] identifier Object to check
 * @retval true iff object named $identifier is locked
 */
bool LockManager::is_locked(const char* identifier)
{
	profiler->function_start();
	profiler->function_sleep();
	pthread_mutex_lock(&(this->lock));
	profiler->function_wakeup();
	bool result = (this->locked_objects->find(std::string(identifier)) != this->locked_objects->end());
	pthread_mutex_unlock(&(this->lock));
	profiler->function_end();
	return result;
}
