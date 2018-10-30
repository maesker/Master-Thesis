/**
 * @file JournalManager.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.07.2011
 */

#ifndef JOURNALMANAGER_H_
#define JOURNALMANAGER_H_
#include <iostream>
#include <map>
#include "Journal.h"
#include "mm/storage/storage.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

class JournalManager
{

public:
    int32_t remove_journal(InodeNumber journal_id);
    Journal* create_server_journal();
    Journal* get_server_journal();
    Journal* create_journal(InodeNumber inode_id);
    Journal* get_journal(InodeNumber inode_id);

    /*Returns reference to the journal manager.
     *
     * This method is needed to get a reference to the singleton instance.
     */
    static JournalManager* get_instance()
    {
    	if(instance == NULL)
    		instance = new JournalManager();
        return instance;
    }



    void recover_journals(const vector<InodeNumber>& partitions);
    int32_t set_storage_abstraction_layer(StorageAbstractionLayer *sal);

    InodeNumber get_open_operations(uint64_t operation_id, vector<Operation>& operations);

    void get_journals(set<InodeNumber> journal_set) const;

    int32_t inode_exists(InodeNumber inode_number, InodeNumber& root_number);
  
private:
    static JournalManager* instance; /* < Singleton instance of JournalManager. */
    
        StorageAbstractionLayer *sal;
    Logger *log;  
    Pc2fsProfiler *profiler;

    Journal* server_journal; /* < Journals the globals server operations. */
    map<InodeNumber, Journal*> journals; /* < list of journals (or subtrees) present in the current MDS */
    
    JournalManager();

    /*
     * Copy constructor of JournalManager
     *
     * @param jm - JournalManager whose values are copied into the new object
     *
     * Private copy constructor to ensure that no other class can use it. It is not used inside the class to ensure that only one single
     * instance of the class is available.
     */
    JournalManager(const JournalManager& jm);
};

#endif /* JOURNALMANAGER_H_ */
