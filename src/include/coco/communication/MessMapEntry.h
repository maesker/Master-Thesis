#ifndef MESSAGEMAPPING_H_
#define MESSAGEMAPPING_H_

#include "CommunicationFailureCodes.h"
#include "CommonCommunicationTypes.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"


#include <pthread.h>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

/** MessMapEntry class represents a message map entry.
 *
 * It provides synchronization between a sender and receiver.
 * It contains a set of extracted massages, where reply messages can be put.
 *
 * @author Viktor Gottfried
 */

class MessMapEntry
{
public:

    /**
     * Constructor of MessMapEntry.
     *
     * Sets up the mutex, the condotion variable and the creation time.
     */
    MessMapEntry()
    {

        Pc2fsProfiler::get_instance()->function_start();
        garbage = false;
        assigned = false;
        waiting = false;
        mutex = PTHREAD_MUTEX_INITIALIZER;
        cond = PTHREAD_COND_INITIALIZER;
        time_stamp = boost::posix_time::second_clock::universal_time();
        //time_stamp.
        Pc2fsProfiler::get_instance()->function_end();
    };

    /**
     * Destructor of MessMapEntry.
     *
     * Destroys the mutex and condition variable.
     */
    virtual ~MessMapEntry()
    {
        Pc2fsProfiler::get_instance()->function_start();
        if (garbage)
            for (int i = 0; i<msg_list->size(); i++)
                free(msg_list->at(i).data);

        msg_list = NULL;
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
        Pc2fsProfiler::get_instance()->function_end();
    };

    /**
     * Check if a MessMapEntry is assigned.
     * @return true if it is assigned; otherwise false.
     */
    bool is_assigned() const
    {

        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        bool ret = assigned;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return ret;
    };

    /**
     * Set the assigned state.
     * @param assigned The boolean state value.
     */
    void set_assigned(bool assigned)
    {
        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        this->assigned = assigned;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
    };

    /**
     * Wait until all messages arrives.
     * This function block until all messages arrived
     * or the timeout exceeds.
     * @param timeout Timeout value.
     * @return true if all messages arrived; false if the timeout exceeds.
     */
    bool wait_for_message(int timeout)
    {
        Pc2fsProfiler::get_instance()->function_start();
        bool ret;
        int cond_ret;
        struct timespec absolute_time;


        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();

        waiting = true;


        absolute_time.tv_sec = time(NULL) + timeout;
        absolute_time.tv_nsec = 0;

        Pc2fsProfiler::get_instance()->function_sleep();
        cond_ret = pthread_cond_timedwait(&cond, &mutex, &absolute_time);
        Pc2fsProfiler::get_instance()->function_wakeup();

        if ( cond_ret != 0)
            ret = false;
        else
            ret = true;

        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return ret;
    };

    /**
     * Wake up a thread, that is waiting for this MessMapEntry @see wait_for_message(int).
     * return 0
     */
    int wake_up()
    {
        Pc2fsProfiler::get_instance()->function_start();
        pthread_cond_signal(&cond);
        Pc2fsProfiler::get_instance()->function_end();
        return 0;
    };

    std::vector<ExtractedMessage> *get_msg_list() const
    {

        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        std::vector<ExtractedMessage>* ret = msg_list;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();

        return ret;
    };

    /**
     * Sets the list of @see ExtractedMessage.
     * This list will be used for the incoming replies.
     * @pram msg_list Pointer to the vector of @see ExtractedMessage.
     */
    void set_msg_list(std::vector<ExtractedMessage> *msg_list)
    {
        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        this->msg_list = msg_list;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
    };

    /**
     * Get the number of expected messages.
     * The number of expected messages must be specified on the creation of a MessMapEntry.
     * @return The number of expected messages.
     */
    int32_t get_expected_msgs() const
    {
        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        int32_t ret = expected_msgs;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return ret;
    };

    /**
     * Set the number of expected messages.
     * @param expected_msgs The number of expected messages.
     */
    void set_expected_msgs(int32_t expected_msgs)
    {
        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        this->expected_msgs = expected_msgs;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
    };

    /**
     * Get the time stamp.
     * The time stamp defines the creation time of a MessMapEntry object.
     * @return The time stamp.
     */
    boost::posix_time::ptime get_time_stamp()
    {
        Pc2fsProfiler::get_instance()->function_start();
        boost::posix_time::ptime ret_time;
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        ret_time = time_stamp;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return ret_time;
    };

    /*
     * Try to lock the mutex.
     * The MessMapEntry is already thread-safe, so please use the normal methods, without locking the object from outside.
     * This method is for special cases, like the garbage collection.
     * @return true if the mutex was locked; otherwise false.
     */
    bool try_lock()
    {
        if (pthread_mutex_trylock(&mutex) != 0)
        {
            Pc2fsProfiler::get_instance()->function_end();
            return false;
        }
        else
        {
            Pc2fsProfiler::get_instance()->function_end();
            return true;
        }
    };

    /**
     * Unlocks the mutex.
     * Must be only used after the try_lock() method and only if the lock was successful.
     */
    void unlock()
    {
        pthread_mutex_unlock(&mutex);
    };

    /**
     * Check if a thread is an the waiting state.
     * @return true if a thread is waiting for this MessMapEntry object; otherwise false.
     */
    bool is_waiting()
    {
        Pc2fsProfiler::get_instance()->function_start();
        bool ret;
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        ret = waiting;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return ret;
    };

    /**
     * Checks if a object was marked by the garbage collection.
     * @return true if the object was marked by the garbage collection; otherwise false.
     */
    bool is_garbage()
    {
        Pc2fsProfiler::get_instance()->function_start();
        bool ret;
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        ret = garbage;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return ret;
    };

    /**
     * Sets the garbage collection state.
     * param garbage The state value.
     */
    void set_garbage(bool garbage)
    {
        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        this->garbage = garbage;
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
    };

private:
    bool waiting; /* < Specifies if someone is waiting for the condition variable.*/
    bool garbage; /* < Specifies if the entry is markes by the garbage collection. */
    bool assigned;

    int32_t expected_msgs; /* < Specefies the number of expected messages. */

    std::vector<ExtractedMessage> *msg_list; /* < Set of messages */
    boost::posix_time::ptime time_stamp; /* < Time stamp defines the creation time */

    mutable pthread_mutex_t mutex; /* < mutex */
    pthread_cond_t cond; /* < condition variable */

};

#endif /* MESSAGEMAPPING_H_ */
