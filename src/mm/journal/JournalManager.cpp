/**
 * @file JournalManager.cpp
 * @class  JournalManager
 * @author Viktor Gottfried
 * @data 19.04.2011
 *
 * @brief Represents the journal manager.
 *
 * The JournalManager class is the main journal access interface.
 * Each metadata server will have a JournalManager which will contain
 * the reference to all the journals on the server.
 */

#include <string.h>
#include <sstream>
#include <iostream>
#include <list>

#include "mm/journal/JournalManager.h"
#include "mm/journal/InodeCache.h"
#include "mm/journal/Journal.h"
#include "mm/journal/JournalException.h"
#include "mm/journal/JournalRecovery.h"

#include "global_types.h"

using namespace std;

JournalManager *JournalManager::instance = NULL;

/*
 * @brief Constructor of JournalManager.
 */
JournalManager::JournalManager()
{
	server_journal = NULL;
        this->profiler = Pc2fsProfiler::get_instance();
	log = new Logger();
	log->set_log_level( 1 );
	log->set_console_output( (bool) 0 );
	log->set_log_location( std::string( DEFAULT_LOG_DIR ) + "/journalmanager" + ".log" );
	log->debug_log("Journal Manager created!");
}

/**
 * @brief Creates an new journal in the journal manager.
 * @param inode_id The inode_id of, must be unique
 * @return Reference to the created journal.
 * @throws JournalException if the sal was not set.
 */
Journal* JournalManager::create_journal(InodeNumber inode_id)
{
	log->debug_log("inode id: %llu.", inode_id);
	if ( get_journal( inode_id ) != NULL)
	{
		log->debug_log("A journal with the id=%llu already exists.", inode_id);
		throw JournalException( "A journal with the id already exists." );
	}
	if ( this->sal != NULL)
	{
		Journal* journal = new Journal( inode_id );

		/** Init storage objects for this journal */
		EmbeddedInodeLookUp *embedded_inode_lookup = new EmbeddedInodeLookUp( this->sal, inode_id );

		journal->set_inode_io( embedded_inode_lookup );
		journal->set_sal( this->sal );

		/** Init storage objects for this journal done */
		journals.insert( unordered_map<InodeNumber, Journal*>::value_type( inode_id, journal ) );
		//journal->start();
		return journal;
	}
	else
	{
		throw JournalException( "Storage Abstraction Layer not registered." );
	}
}

/**
 * @brief Gets the server journal.
 * @return The server journal.
 */
Journal* JournalManager::get_server_journal()
{
	return this->server_journal;
}

/**
 * @brief Creates the journal, that is responsible for the global server operations.
 * The server journal is not related no any subtree and has the id number 0.
 * @return Pointer to the journal.
 * @throws JournalException if the sal was not set or the journal already exists.
 */
Journal* JournalManager::create_server_journal()
{

	if ( server_journal != NULL)
	{
		throw JournalException( "Server journal already exists!" );
	}
	if ( this->sal != NULL)
	{
		server_journal = new Journal( 0 );
		server_journal->set_sal( this->sal );
		// the server journal does not writes any inodes
		server_journal->set_inode_io( NULL );
		return server_journal;
	}
	else
	{
		throw JournalException( "Storage Abstraction Layer not registered." );
	}
}

/**
 * @brief Sets the @see StorageAbstractionLayer object.
 * @param sal Pointer to the @see StorageAbstractionLayer object.
 */
int32_t JournalManager::set_storage_abstraction_layer(StorageAbstractionLayer *sal)
{
	if ( sal != NULL)
	{
		this->sal = sal;
		return 0;
	}
	return 1;
}

/**
 * @brief Removes the journal from the journal manager.
 * @param inode_id The id of the journal to remove.
 * @return 0 if the operation was successful, -1 if the manager does not contain a journal with the given id.
 */
int32_t JournalManager::remove_journal(InodeNumber journal_id)
{
	map<InodeNumber, Journal*>::iterator it;

	it = journals.find( journal_id );
	if ( it == journals.end() )
		return -1;

	Journal* j;
	j = it->second;
	j->stop();
	delete j;

	journals.erase( it );

	return 0;
}

/**
 * @brief Get the journal from the journal manager using the inode_id.
 * @param inode_id Id of the journal.
 * @return Reference to the journal, if the journal with the given id exists,
 * otherwise NULL.
 */
Journal* JournalManager::get_journal(InodeNumber inode_id)
{
    this->profiler->function_start();
	Journal* journal;
	map<InodeNumber, Journal*>::const_iterator it;

	if ( (it = journals.find( inode_id )) == journals.end() )
		journal = NULL;
	else
		journal = (*it).second;
    this->profiler->function_end();
	return journal;
}

/**
 * @brief Starts the journal recovery.
 * @param partitions A vector of all partitions handling by the system.
 */
void JournalManager::recover_journals(const vector<InodeNumber>& partitions)
{
	StorageAbstractionLayer *sal; // TODO just a dummy, set a correct instace of the sal somewhere before!
	JournalRecovery jr( sal );
	jr.start_recovery( journals, partitions );
}

/**
 * @brief Tries to get all open operations from a journal.
 * It must iterate through all journals that are maintained by the journal manager.
 * @param[in] operation_id The id of the wanted operation.
 * @param[out] operations Vector to store the wanted operations.
 * @return The root inode id of the that identifies the journal where the operations are stored, if a journal contains
 * open operations with the given operation id, otherwise INVALID_INODE_ID (=0) is returned.
 */
InodeNumber JournalManager::get_open_operations(uint64_t operation_id, vector<Operation>& operations)
{
	int32_t rv = 0;
	map<InodeNumber, Journal*>::iterator it;

	for ( it = journals.begin(); it != journals.end(); it++ )
	{
		rv = it->second->get_all_operations( operation_id, operations );
		if ( rv != 0 )
			return it->first;
	}

	return INVALID_INODE_ID;
}

/**
 * @brief Gets all journal id which are provided by the journal mananger.
 * @param[out] journal_set Set of journals.
 */
void JournalManager::get_journals(set<InodeNumber> journal_set) const
{
	map<InodeNumber, Journal*>::const_iterator cit;
	for ( cit = journals.begin(); cit != journals.end(); cit++ )
	{
		journal_set.insert( cit->first );
	}
}

/**
 * Checks whether the inode exists on this server and gets the number of the root node.
 * @param inode_number The inode number to check.
 * @param root_number
 * @return 0 if the einode is present, -1 if not. -2 the inode number was present, but is tagged as deleted now.
 */
int32_t JournalManager::inode_exists(InodeNumber inode_number, InodeNumber& root_number)
{
	int32_t rtrn = -1;
	map<InodeNumber, Journal*>::iterator it;

	it = journals.begin();
	while ( it != journals.end() && rtrn != 0 )
	{
		int32_t result = it->second->inode_exists( inode_number );
		if ( result == 0 )
		{
			root_number = it->first;
			rtrn = result;
		}
		else if ( result == -2 )
		{
			root_number = it->first;
			rtrn = result;
		}
		++it;
	}
	return rtrn;
}
