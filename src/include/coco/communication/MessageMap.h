#ifndef MESSAGEMAP_H_
#define MESSAGEMAP_H_

#include "MessMapEntry.h"
#include "CommunicationFailureCodes.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"
#include <unordered_map>
#include <pthread.h>

/**MessageMap class represents a map, which maps a message id to the corresponding message map entries.
 *
 * A MessageMap is holding a set of messages, that can be identified with the message id.
 *
 *
 * @author Viktor Gottfried
 */

class MessageMap
{
public:

    /**
     * Constructor of MessageMap.
     *
     * Sets up the mutex.
     */
    MessageMap()
    {
        Pc2fsProfiler::get_instance()->function_start();
        mutex = PTHREAD_MUTEX_INITIALIZER;
        Pc2fsProfiler::get_instance()->function_end();
    };

    /**
     * Destructor of MessageMap.
     *
     * Destroys the mutex.
     */
    virtual ~MessageMap()
    {
        Pc2fsProfiler::get_instance()->function_start();
        pthread_mutex_destroy(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
    };

    /**
     * Find the entry with the corresponding message ID.
     * @param message_id Message ID.
     * @return Returns the entry; NULL if the map does not contain an entry with the given id.
     */
    MessMapEntry* find_entry(uint64_t message_id)
    {
        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();

        MessMapEntry* e;
        unordered_map<uint64_t, MessMapEntry*>::const_iterator it;

        if ((it = m_map.find(message_id)) == m_map.end())
            e = NULL;
        else
            e = (*it).second;

        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return e;
    };

    /**
     *	Add a new message to the map.
     *	@param message_id Message ID.
     *	@param message Pointer to the MessageMapEntry.
     *	@return 0.
     */
    int32_t add_message(uint64_t message_id, MessMapEntry* message)
    {
        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        m_map.insert(unordered_map<uint64_t, MessMapEntry*>::value_type(message_id,message));
        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return 0;
    };

    /**
     * Remove a message from the map.
     * @param message_id Id of the message to remove.
     * @return 0 remove successful; 1 otherwise
     */
    int32_t remove_message(uint64_t message_id)
    {

        Pc2fsProfiler::get_instance()->function_start();
        Pc2fsProfiler::get_instance()->function_sleep();
        pthread_mutex_lock(&mutex);
        Pc2fsProfiler::get_instance()->function_wakeup();
        int32_t value;
        if (m_map.erase(message_id) == 1)
            value = 0;
        else
            value = 1;

        pthread_mutex_unlock(&mutex);
        Pc2fsProfiler::get_instance()->function_end();
        return value;
    };

    /**
     * Get the map set.
     * @return Reference to the map.
     */
    std::unordered_map<uint64_t, MessMapEntry*>& get_m_map()
    {
        return m_map;
    };

    /**
     * Locks the mutex.
     * Must be unlocked by @see unlock().
     */
    void lock()
    {
        pthread_mutex_lock(&mutex);

    };

    /**
     * Unlocks the mutex.
     * Must be called after @see lock().
     */
    void unlock()
    {
        pthread_mutex_unlock(&mutex);

    };

private:
    std::unordered_map<uint64_t, MessMapEntry*> m_map; 	/* < Set of messages entries.*/
    mutable pthread_mutex_t mutex;					/* < Mutex */
};

#endif /* MESSAGEMAP_H_ */
