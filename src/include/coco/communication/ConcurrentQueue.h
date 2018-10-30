/**
 * \file ConcurrentQueue.h
 *
 * \author Benjamin Raddatz	Marie-Christine Jakobs
 *
 * \brief Realizes a queue which can be accessed by multiple threads without any harm.
 *
 * If more than one thread wants to modify the queue at the same time, this will be excluded by synchronization. So the queue will be modified
 * in an arbitrary but sequential order.
 */
#ifndef CONCURRENTQUEUE_H_
#define CONCURRENTQUEUE_H_

#include <queue>
#include <sstream>
#include <pthread.h>
#include <ctime>
#include "CommunicationLogger.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

/**
 * Defines the value which is thrown in case of a failure regarding a mutex lock or a failure in mutex creation.
 */
#define MUTEXFAIL 1

/**
 * Defines the value which is thrown in case of a failure regarding a condition wait or a failure in condition variable creation.
 */
#define CONDITIONVARFAIL 2

template<typename Data>
/**
 * Implements a generic queue containing elements of a certain type defined during variable declaration. The queue is thread safe because
 * concurrent modification is excluded.
 */
class ConcurrentQueue
{
private:
    /**
     * Data structure which contains the elements which can be accessed in FIFO way.
     */
    std::queue<Data> queue;
    /**
     * Used for synchronization. At any time only one thread is able to own the lock. Synchronization will only be used for modification.
     */
    pthread_mutex_t modify_mutex;
    pthread_mutex_t waitpop_mutex;
    /**
     * Used to block a thread and unlock the mutex at the same time to allow to add elements if a thread waits for an element to take from
     * the queue.
     */
    pthread_cond_t condition_var;
    /**
     * Saves the point in time when the last pop was done.
     */
    time_t last_pop;
    /**
     * Indicates if at least one pop has been done yet. It is set to false in the constructor and to true if a pop is done.
     */
    bool at_least_one_pop_done;
    
    bool condvarflag;

#ifdef LOGGING_ENABLED
    Logger* logging_instance;
#endif

public:
    /*!\brief Constructor of "ConcurrentQueue"
     *
     * Sets up the mutex and the condotion variable,
     */
    ConcurrentQueue()
    {
        Pc2fsProfiler::get_instance()->function_start();
        condvarflag=false;
        //set up logging instance
#ifdef LOGGING_ENABLED
        logging_instance = CommunicationLogger::get_instance();
#endif
        int failure = pthread_mutex_init(&modify_mutex, NULL);
        failure = pthread_mutex_init(&waitpop_mutex, NULL);
        
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream message;
            message<<"Mutex cannot be created due to "<<failure;
            (*logging_instance).error_log(message.str().c_str());
#endif
            Pc2fsProfiler::get_instance()->function_end();
            throw MUTEXFAIL;
        }
        failure = pthread_cond_init(&condition_var, NULL);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream message2;
            message2<<"Condition variable cannot be created due to "<<failure;
            (*logging_instance).error_log(message2.str().c_str());
#endif
            Pc2fsProfiler::get_instance()->function_end();
            throw CONDITIONVARFAIL;
        }
        at_least_one_pop_done = false;
        Pc2fsProfiler::get_instance()->function_end();
    }
    /**
     *\brief Destructor of "ConcurrentQueue"
     *
     * Destroys the mutex and condition variable.
     */
    ~ConcurrentQueue()
    {
        Pc2fsProfiler::get_instance()->function_start();
        //tidy up
        int failure = pthread_mutex_destroy(&modify_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message;
            warning_message<<"Mutex cannot be destroyed properly due to "<<failure;
            (*logging_instance).warning_log(warning_message.str().c_str());
        }
#endif
        failure = pthread_cond_destroy(&condition_var);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message2;
            warning_message2<<"Condition variable cannot be destroyed properly due to "<<failure;
            (*logging_instance).warning_log(warning_message2.str().c_str());
        }
#endif
        Pc2fsProfiler::get_instance()->function_end();
    }
    /*!\brief Allows to push one object into the queue
     *
     * Locks the mutex to assure consistency. After pushing frees the mutex and contacts threads waiting for objects by the condition variable.
     */
    void push(Data const& data)
    {
        Pc2fsProfiler::get_instance()->function_start();
        int failure;
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = pthread_mutex_lock( &modify_mutex );
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message;
            error_message<<"Mutex cannot be aquired due to "<<failure;
            (*logging_instance).error_log(error_message.str().c_str());
#endif
            Pc2fsProfiler::get_instance()->function_end();
            throw MUTEXFAIL;
        }
        queue.push(data);
        condvarflag=true;
        failure = pthread_cond_signal(&condition_var);
        failure = pthread_mutex_unlock(&modify_mutex);
        Pc2fsProfiler::get_instance()->function_end();
    }
    /*!\brief Checks wether the queue is empty or not
     *
     *\return true if the queue does not contain any elements and false otherwise
     */
    bool empty() const
    {
        return queue.empty();
    }
    /*!\brief Tries to pop an element without blocking
     *
     * If an element is available it is assigned to the parameter popped_value otherwise nothing is assigned to this parameter. The return type
     * indicates if popping was successful.
     *
     * \param popped_value output parameter which will contain the popped element if method is successful
     * \return false if the queue is empty. If not puts the first element from the queue at the given adress
     */
    bool try_pop(Data& popped_value)
    {
        Pc2fsProfiler::get_instance()->function_start();
        int failure;
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = pthread_mutex_lock( &modify_mutex );
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message;
            error_message<<"Mutex cannot be aquired due to "<<failure;
            (*logging_instance).error_log(error_message.str().c_str());
#endif
            Pc2fsProfiler::get_instance()->function_end();
            throw MUTEXFAIL;
        }
        if (queue.empty())
        {
            failure = pthread_mutex_unlock(&modify_mutex);
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message;
                warning_message<<"Mutex cannot be unlocked due to "<<failure;
                (*logging_instance).warning_log(warning_message.str().c_str());
            }
#endif
            Pc2fsProfiler::get_instance()->function_end();
            return false;
        }
        popped_value=queue.front();
        queue.pop();
        // set time for last pop
        at_least_one_pop_done = true;
        last_pop = time(NULL);
        failure = pthread_mutex_unlock(&modify_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message2;
            warning_message2<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message2.str().c_str());
        }
#endif
        Pc2fsProfiler::get_instance()->function_end();
        return true;
    }
    /*!\brief Pop an element with blocking until an element is available
     *
     * Puts the first element from the queue at the given adress. If the queue is empty it blocks and waits until an element is put into the
     * queue.
     *
     * \param popped_value output parameter which will contain the popped element
     */
    void wait_and_pop(Data& popped_value)
    {
        Pc2fsProfiler::get_instance()->function_start();
        int failure;
        Pc2fsProfiler::get_instance()->function_sleep();
        
        failure = pthread_mutex_lock( &waitpop_mutex );
        failure = pthread_mutex_lock( &modify_mutex );
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message;
            error_message<< "Mutex cannot be aquired due to "<<failure;
            (*logging_instance).error_log(error_message.str().c_str());
#endif
            Pc2fsProfiler::get_instance()->function_end();
            throw MUTEXFAIL;
        }
        if (queue.empty())
        {
           // pthread_mutex_unlock( &waitpop_mutex );
            condvarflag=false;
            while (!condvarflag)
            {
                failure = pthread_cond_wait(&condition_var, &modify_mutex);
            }
           // failure = pthread_mutex_lock( &waitpop_mutex );
            condvarflag=false;
        }
        popped_value=queue.front();
        queue.pop();
        // set time for last pop
        at_least_one_pop_done = true;
        last_pop = time(NULL);
        failure=pthread_mutex_unlock(&modify_mutex);
        failure = pthread_mutex_unlock( &waitpop_mutex );
        
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message;
            warning_message<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message.str().c_str());
        }
#endif
        Pc2fsProfiler::get_instance()->function_end();
    }

    /*!\brief Tries to pop an element, blocks until the timeout exceeds.
     *
     * Tries to put the first element from the queue at the given address. If the queue is empty it blocks and waits until an element is put into the
     * queue or the timeout exceeds
     *
     * \param popped_value output parameter which will contain the popped element
     * \param timeout Timeout value in seconds.
     * \return Returns true if an element was popped; false if the timeout exceeds.
     */
    bool try_wait_and_pop(Data& popped_value, int timeout)
    {
        Pc2fsProfiler::get_instance()->function_start();
        struct timespec absolute_time;
        int failure;
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = pthread_mutex_lock( &modify_mutex );
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
            Pc2fsProfiler::get_instance()->function_end();
            throw MUTEXFAIL;
        }
        if (queue.empty())
        {
            // pthread_cond_timedwait expects absolute time: get current time + timeout
            absolute_time.tv_sec = time(NULL) + timeout;
            absolute_time.tv_nsec = 0;

            Pc2fsProfiler::get_instance()->function_sleep();
            failure = pthread_cond_timedwait(&condition_var, &modify_mutex, &absolute_time);
            Pc2fsProfiler::get_instance()->function_wakeup();

            if (failure)
            {

                if (failure == ETIMEDOUT )
                {
                    pthread_mutex_unlock(&modify_mutex);
                    Pc2fsProfiler::get_instance()->function_end();
                    return false;
                }
                else
                    Pc2fsProfiler::get_instance()->function_end();
                throw CONDITIONVARFAIL;
            }
        }
        popped_value=queue.front();
        queue.pop();
        // set time for last pop
        at_least_one_pop_done = true;
        last_pop = time(NULL);
        failure=pthread_mutex_unlock(&modify_mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return true;
    }

    /**
     *\brief Returns the elapsed time in seconds since the last pop
     *
     *\return 0 if no pop has been done yet otherwise the elapsed time in seconds since the last pop
     *
     * Computes the difference between the current time and the time of the last pop.
     */
    double get_elapsed_time_after_last_pop()
    {
        Pc2fsProfiler::get_instance()->function_start();
        if (at_least_one_pop_done)
        {
            Pc2fsProfiler::get_instance()->function_end();
            return difftime(time(NULL) ,last_pop);
        }
        Pc2fsProfiler::get_instance()->function_end();
        return 0.0;
    }
};

#endif //#ifndef CONCURRENTQUEUE_H_

