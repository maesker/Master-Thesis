#include "mm/mds/SubtreeManager.h"

/** 
 * @file SubtreeManager.cpp
 * @class SubtreeManager
 * @author Markus Maesker, maesker@gmx.net
 * @date 21.07.11
 * 
 * @brief Manages the Subtrees the MetadataServer is responsible for.
 * 
 * @todo Testing
 * 
 * 
 * This class manages the partition handling of the MDS. It does not handle 
 * the Partition management on the storagelayer.
 * 
 * Startup:
 * During startup the subtreemanager(SM) extracts the subtrees this mds is 
 * responsible for from the mlt and creates journal instances for each of these
 * subtrees. 
 * 
 * Normal MDS Interaction:
 * The main MDS <-> Subtreemanager interaction is the journal lookup for a 
 * provided inode number. The Subtreemanager acquires this simply by asking the
 * journal manager (JM). The reason the SM is part of the workflow is the 
 * repartitioning process. During this process, the subtree that is going to 
 * be migrated to another MDS needs to be blocked.
 * 
 * Re-partitioning:
 * This is the most important part of the Subtreemanager. If a repartitoning 
 * request on an existing subtree arrives, that subtree is blocked and all
 * accesses are rejected. The SM keeps a map <pid,inodenumber> to identify 
 * threads that are still working on this subtree. So after requests are 
 * rejected and all working threads finished work on that subtree, the journal
 * is flushed and the partition is remove.
 * 
 * If a new Subtree is being created, only a new journal is created.
 *
 * @todo The SM has a routine to merge to subtrees, further investigation hase to be
 * done on the granularity of the file system partitioning
 * */

/**
 * @brief Create new journal instance with root inode number.
 * @param[in] p_inum pointer to root inode number.
 * @throw SubtreeManagerException
 * @return subtree_manager_global_ok if ok. 
 * subtree_manager_error if not.
 * 
 * Creates a new Joural using the JournalManager. If creation was 
 * successful, the Journals start function is called.
 * */
int32_t SubtreeManager::create_journal_instance(InodeNumber *p_inum)
{
    p_profiler->function_start();
    int32_t rc=subtree_manager_global_ok;
    try
    {   
        // create new journal instance with provided inode number as 
        // root object.
        Journal *p_new_journal= p_jm->create_journal(*p_inum);
        if (p_new_journal == NULL)
        {
            log->error_log("Failed to create new journal instance for inode:%d",*p_inum);
            rc = subtree_manager_error;
        }
        else
        {
            log->debug_log("New journal with root_inode_number %d created.",*p_inum);
            p_new_journal->start();
        }
    }
    catch (JournalException e)
    {
        std::string s = "JournalManager exception thrown:";
        s+=e.get_msg();
        log->error_log(s.c_str());
        p_profiler->function_end();
        throw SubtreeManagerException(s.c_str());
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Create_journal_instance:Unknown exception thrown");
    }
    p_profiler->function_end();
    return rc;
} // end of SubtreeManager::create_journal_instance

/**
 * @brief initializes a new subtree
 * @param[in] root inode number of the new  subtrees root object
 * @throw SubtreeManagerException
 * @return integer 0 if ok.
 * 
 * Initializes a new Subtree b< calling create_journal_instance.
 * */
int32_t SubtreeManager::initialize_new_subtree(InodeNumber root)
{
    p_profiler->function_start();
    int32_t rc=0;
    try
    {
        // creates new journal instance for the provided inode number
        rc=create_journal_instance(&root);
        log->debug_log("New journal instance created. rc:%d",rc);
        if (!rc)
        {
            root_inode_numbers.insert(root);
            log->debug_log("root inode number added=%d",root);
        }
        else
        {
            log->debug_log("Error creating journal instance.rc=%d",rc);
        }
    }
    catch( JournalException e)
    {
        std::string s ="JournalException thrown";
        s+= e.get_msg().c_str();
        log->error_log(s.c_str());
        p_profiler->function_end();
        throw SubtreeManagerException(s.c_str());
    }
    catch( SubtreeManagerException e)
    {
        log->error_log("SubtreeManagerException thrown:%s.",e.what());
        p_profiler->function_end();
        throw e;
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Initialize_new_subtree:Unknown exception thrown.");
    }
    p_profiler->function_end();
    return rc;
} // end of SubtreeManager::initialize_new_subtree

/**
 * @brief read mlt and initialize the subtrees this mds is responsible for.
 * @throw SubtreeManagerException
 * @return 0 ok, nonzero failure
 * 
 * */
int32_t SubtreeManager::initialize_subtrees()
{
    p_profiler->function_start();
    // acquire lock to initialize subtrees.
    p_profiler->function_sleep();
    pthread_mutex_lock(&init_mutex);
    p_profiler->function_wakeup();
    int32_t rc=subtree_manager_global_ok;
    try
    {
        if (!initialized)
        {
            log->debug_log("Initialize subtrees.");
            vector<InodeNumber> partitions;
            // get partitions, this server is responsible for
            p_mlt_handler->get_my_partitions(partitions);

            if( partitions.size() == 0 )
            {
                log->error_log( "No partitions found!" );
            }
            vector<InodeNumber>::iterator vit;
            for (vit = partitions.begin(); vit != partitions.end(); ++vit)
            {   
                // get path of the partition
                std::string path = p_mlt_handler->get_path(*vit);
                log->debug_log("Found subtree this server is responsible for:");
                log->debug_log("Inum:%u, Path:%s.",*vit,path.c_str());
                // test whether this path is the root object
                if (!(strcmp("/",path.c_str())))
                {
                    // this server is responsible for the file systems 
                    // root object
                    root_flag = true;
                    log->debug_log("This server is responsible for the root object");
                }
                // create the new subtree instance
                rc=initialize_new_subtree(*vit);
            }
            log->debug_log("Initialize subtrees done. rc:%d.",rc);
            // all subtrees initialized
            initialized=true;
        }
        else
        {
            log->error_log("Subtrees already initialized.");
        }
    }
    catch( JournalException e)
    {
        // release mutex and throw exception
        pthread_mutex_unlock(&init_mutex);
        std::string s = "Journal exception thrown";
        s+=e.get_msg().c_str();
        log->error_log(s.c_str());
        p_profiler->function_end();
        throw SubtreeManagerException(s.c_str());
    }
    catch( SubtreeManagerException e)
    {
        // release mutex and throw exception
        pthread_mutex_unlock(&init_mutex);
        log->error_log("Subtreemanager exception thrown:%s.",e.what());
        p_profiler->function_end();
        throw e;
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        // release mutex and throw exception
        pthread_mutex_unlock(&init_mutex);
        p_profiler->function_end();
        throw SubtreeManagerException("Initialize_subtrees:Unknown exception thrown");
    }
    // release mutex and throw exception
    pthread_mutex_unlock(&init_mutex);
    p_profiler->function_end();
    return rc;
} // end of SubtreeManager::initialize_subtrees



/** 
 * @brief Creates a new subtree and returns
 * @param[in] p_inum inode number of the root fs object of the new subtree
 * @throw SubtreeManagerException
 * @return integer: 0 if successful
 * */
int32_t SubtreeManager::handle_migration_create_partition(InodeNumber *p_inum)
{
    p_profiler->function_start();
    int32_t rc=subtree_manager_global_ok;
    try
    {
        if (p_inum != NULL)
        {
            rc = create_journal_instance(p_inum);
            // check if this operation was successful.
            if (!rc)
            {
                log->debug_log("New subtree created successfully.");
            }
            else
            {
                log->warning_log("Error while creating the new subtree");
                rc = subtree_manager_new_subtree_create_error;
            }
        }
        else
        {
            log->error_log("No inode number provided.");
            rc = subtree_manager_no_inode_number_provided;
        }
    }
    catch (SubtreeManagerException e)
    {
        log->error_log(e.what());
        p_profiler->function_end();
        throw e;
    }
    p_profiler->function_end();
    return rc;
}

/** 
 * @brief Traverse the MLT and check for each subtree this MDS is 
 * responsible for, whether it is also responsible for a child entry of
 * that subtree. This means, that the child entry can be removed and 
 * hence is merged into the parent tree. If this is done, the journal 
 * of the child is written back and deleted.
 * @throw SubtreeManagerException
 * @return 1 if a subtree was merged, 0 if nothing has changed.
 * */
int16_t SubtreeManager::investigate_subtree_merge_potential()
{
    p_profiler->function_start();
    int16_t subtrees_merged = 0;
    InodeNumber root = 1;
    // start recursion with root inode object
    try
    {
        investigate_subtree_merge_potential_worker(&root, &subtrees_merged);
        log->debug_log("Has merge been performed=%d.",subtrees_merged);
    }
    catch (SubtreeManagerException e)
    {
        log->error_log(e.what());
        p_profiler->function_end();
        throw e;
    }
    p_profiler->function_end();
    return subtrees_merged;
}
 
 
 /**
  * @brief Recursive worker of the investigate_subtree_merge_potential
  * method.
  * @param[in] p_parent pointer to inodenumber representing the mlt
  * entry to check for child entrys
  * @param[out] flag pointer to integer to flag if a merge has been 
  * performed.
  * @throw SubtreeManagerException
  * @return void
  * */
void SubtreeManager::investigate_subtree_merge_potential_worker(InodeNumber *p_parent, int16_t *flag)
{
    p_profiler->function_start();
    // initialize variables used during investigation
    int16_t rc=-1;
    bool i_am_parent = false;
    vector<InodeNumber> children;
    log->debug_log("Investigate merge potential for subtree id:%u.",*p_parent);
    try
    {
        // get the address of this MDS to identify the subtrees of this MDS.
        Server myself = p_mlt_handler->get_my_address();
        log->debug_log("I am Server:%s.",myself.address.c_str());
            
        // get the address of the subtree to check for childs entries
        Server parent_server = p_mlt_handler->get_mds(*p_parent);

        if (!myself.address.compare(parent_server.address))
        {
            // this parent is being served by this MDS
            i_am_parent = true;
        }

        // get all children of the current subtree.
        rc = p_mlt_handler->get_children(*p_parent, children);
        if (rc==1)
        {
            // for all child entries check whether they are being server by
            // this mds too.
            log->debug_log("Got children.");
            vector<InodeNumber>::iterator vit;
            for (vit = children.begin(); vit != children.end(); ++vit)
            {   
                // true if the parent directory is served by this mds.
                if (i_am_parent)
                {
                    // get the address of the child entry
                    Server current_server = p_mlt_handler->get_mds(*vit);
                    log->debug_log("This is Server:%s.",current_server.address.c_str());
                    
                    // check if the parent and the child entry have the same 
                    // address and therefore are being served by the same MDS
                    if (!myself.address.compare(current_server.address))
                    {
                        // both, parent and child, are being served by this 
                        // MDS and can be merged.
                        log->debug_log("Server equal: vit:%d and both on this machine.",*vit);
                        InodeNumber child = *vit;

                        // merge the subtrees
                        rc = merge_two_subtrees(p_parent,&child);
                        if (! rc) 
                        {
                            // merge successful
                            *flag=1;
                        }
                    }
                    else
                    {
                        // the address of the parent and the child did not
                        // match. That means, the child is being served by
                        // another MDS. Recursivly continue to check this
                        // subtree for other subtrees
                        investigate_subtree_merge_potential_worker(&*vit, flag);
                    }
                }
                else
                {
                    // The parent directory is not served on this MDS 
                    // That means this MDS has no right to merge anything 
                    // into this subtree. Recursivly continue to check this
                    // subtree for other subtrees.
                    investigate_subtree_merge_potential_worker(&*vit, flag);
                }
            }        
        }
        else if (rc==0)
        {
            // the parent does not have any children. Quit this recursion.
            log->debug_log("Inode %u has no child partition.",*p_parent);
            rc=0;
        }
        else
        {
            // an error occured while reading the mlt.
            log->warning_log("Could not receive children. MLT handler error. rc=%d",rc);
        }
    }
    catch (SubtreeManagerException e)
    {
        log->error_log(e.what());
        p_profiler->function_end();
        throw e;
    }
    catch (...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Unknown exception thrown.");
    }
    p_profiler->function_end();
}

/**
 * @brief Merge two subtrees.
 * @throw SubtreeManagerException
 * @return integer: 0 on success, failure else
 * */
int32_t SubtreeManager::merge_two_subtrees(InodeNumber *p_parent_inum, InodeNumber *p_subdir)
{
    p_profiler->function_start();
    int32_t rc=-1;
    try
    {
        log->debug_log("Mergeing subtree with inode number %u into parent with inode number %u.",*p_subdir, *p_parent_inum);
        rc = handle_migration_quit_partition(p_subdir);
        if (!rc)
        {
            log->debug_log("Journal of subtree %u flushed and quit. Now removing entry from mlt",*p_subdir);
            rc = p_mlt_handler->handle_remove_entry(*p_subdir);
            if (!rc)
            {
                p_mlt_handler->write_to_file(MLT_PATH);
                log->debug_log("Updated mlt written.");
            }
            log->debug_log("Removed entry %u from mlt. rc=%d.",*p_subdir,rc);
        }
        else
        {
            log->warning_log("Error occured while flushing journal %u. Rc:%d.",*p_subdir,rc);
            rc = subtree_manager_merge_error;
        }
    }
    catch (SubtreeManagerException e)
    {
        log->error_log(e.what());
        p_profiler->function_end();
        throw e;
    }
    catch (...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Unknown exception thrown.");
    }
    p_profiler->function_end();
    return rc;
}


/** 
 * @brief Stop handling of the Partition with inode number p_inum
 * @param[in] p_inum Pointer to InodeNumber, representing the Partition root inode
 * @throw SubtreeManagerException {details look at exception message}
 * @return integer: 0 if ok, failure else
 * @todo test.
 * */
int32_t SubtreeManager::handle_migration_quit_partition(InodeNumber *p_inum)
{
    p_profiler->function_start();
    int32_t rc=0;
    try
    {
        log->debug_log("Quit subtree id %u.",*p_inum);
        Journal *p_journal = get_responsible_journal(p_inum);
        if (p_journal!=NULL)
        {
            InodeNumber journal_root_inum = p_journal->get_journal_id();
            log->debug_log("Quit subtree id %u on journal id %u.",*p_inum, journal_root_inum);
            rc = block_subtree(&journal_root_inum);
            if (!rc)
            {   
                log->debug_log("Subtree locked");
                rc = p_jm->remove_journal(journal_root_inum);
                log->debug_log("Journal with id %u removed. rc:%d",journal_root_inum,rc);
                if (!rc)
                {
                    set<InodeNumber>::iterator it;
                    it = root_inode_numbers.find(journal_root_inum);
                    if (it != root_inode_numbers.end())
                    {
                        root_inode_numbers.erase(it);
                    }
                    else
                    {
                        log->warning_log("Inode number %u was not part of root_inode_numbers. Why is that? ");
                    }
                }
            }
            else
            {
                log->error_log("Error while blocking the subtree journal.");
            }
        }
        else
        {
            log->warning_log("No journal with id:%u found.",*p_inum);
            rc = subtree_manager_no_journal_error;
        }
    }
    catch (SubtreeManagerException e)
    {
        log->debug_log(e.what());
        free_blocked_root_inum();
        p_profiler->function_end();
        throw e;
    }
    catch (...)
    {
        log->error_log("Unknown exception thrown.");
        free_blocked_root_inum();
        p_profiler->function_end();
        throw SubtreeManagerException("Unknown exception thrown.");
    }    
    // free the blocked root inum again.
    free_blocked_root_inum();
    log->debug_log("Released blockage again.");
    p_profiler->function_end();
    return rc;
}

/**
 * @brief Create a new fs subtree.
 * @param[in] p_path char pointer representing the path of the subtree.
 * @param[in] p_inum Pointer to the InodeNumber object. This is the root
 * inode number of the new subtree. 
 * @throw SubtreeManagerException
 * @return int32_t. 0=ok, else represents an error.
 * @todo Are there cleanup operations necessary here?
 * 
 * Acquires a new export id and creates a MLT entry structure.
 * If these operations are successful, a new Journal instance is created.
 * If an error occures, the already performed operations are undone.
 * */
int32_t SubtreeManager::create_new_subtree(InodeNumber *p_inum,
        std::string *p_path)
{
    p_profiler->function_start();
    log->debug_log("Create new subtree inum:%u, path:%s",*p_inum, p_path->c_str());
    int32_t rc = subtree_manager_global_ok;
    int rc_mltadd=0;
    try
    {
        // check for input
        if (p_inum != NULL)
        {
            ExportId_t dummy = 0;
            // add the corresponding entry to the mlt handler
            rc_mltadd=p_mlt_handler->handle_add_new_entry(p_mlt_handler->get_my_address(),dummy,(InodeNumber)*p_inum,p_path->c_str());
            if (rc_mltadd)
            {
                // mlt handler reported an error while adding partition
                log->error_log("Failed to build new mlt_entry for inode:%d",*p_inum);
                rc=subtree_manager_error;
            }
            else
            {
                // successfully added mlt entry. Create journal instance
                rc=create_journal_instance(p_inum);
                if (!rc)
                {
                    root_inode_numbers.insert(*p_inum);
                    log->debug_log("root added=%d",*p_inum);
                }
                else
                {
                    log->debug_log("Error creating journal instance.rc=%d",rc);
                }
            }
        }
        else
        {
            log->error_log("Received an empty inode number pointer");
            rc=subtree_manager_error;
        }
        if (rc)
        {
            // an error occured. Perform cleanup actions.
            log->error_log("Rewinding last operations due to an error.");
            if (rc_mltadd)
            {
                // undo operation at mlt
                log->debug_log("Remove previously created mlt entry.");
                p_mlt_handler->handle_remove_entry(*p_inum);
            }
        }
        else
        {
            // write modified mlt to file
            rc = p_mlt_handler->write_to_file(MLT_PATH);
            if (!rc)
            {
                log->debug_log("Wrote updated mlt to file. rc=%d",rc);
            }
            else
            {
                log->debug_log("Error while writing updated mlt to file. rc=%d",rc);
            }
        }
    }
    catch (SubtreeManagerException e)
    {
        log->error_log(e.what());
        p_profiler->function_end();
        throw e;
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Create_new_subtree:Unknown exception thrown");
    }
    log->debug_log("Create new subtree done. rc:%d",rc);
    p_profiler->function_end();
    return rc;
} // end of SubtreeManager::create_new_subtree

/**
 * @brief Return pointer to the journal instance that is responsible for
 * the subtree that the provided file system object is stored at.
 * @param[in] p_inum pointer to an InodeNumber that specifies the
 * file system object the operation is performed on.
 * @throw SubtreeManagerException
 * @return pointer to journal instance or NULL if inode number invalid
 * */
Journal* SubtreeManager::get_responsible_journal(InodeNumber *p_inum)
{
    p_profiler->function_start();
    Journal *p_j = NULL;
    try
    {
        // get responsible journal instance, p_inum represents the 
        // subtrees root inode number
        p_j = p_jm->get_journal(*p_inum);
        if (p_j!=NULL)
        {
            log->debug_log("Journal instance for inum:%d found.",*p_inum);
            if (p_j->get_journal_id() != blocked_root_inum)
            {
                set_working_on(p_j->get_journal_id());
            }
            else
            {
                log->warning_log("Journal instance for inum:%d is blocked.",*p_inum);
            }
        }
        else
        {
            log->error_log("No Journal instance for inum:%d registered.",*p_inum);
        }
    }
    catch (SubtreeManagerException e)
    {
        log->error_log(e.what());
        p_profiler->function_end();
        throw e;
    }
    catch (JournalException e)
    {
        std::string s = "JournalException:";
        s+=e.get_msg().c_str();
        log->error_log(s.c_str());
        p_profiler->function_end();
        throw SubtreeManagerException(s.c_str());
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Unknown exception thrown");
    }
    p_profiler->function_end();
    return p_j;
} // end of SubtreeManager::get_responsible_journal

/** 
 * @brief Block an subtree in order to perform migration operations. 
 * After that wait for other threads that still work on that journal to
 * finish.
 * @param[in] inum pointer to root inode number of the subtree to block
 * @throw SubtreeManagerException
 * @return 0 means blocked successful 
 * 
 * @todo Improve busy journal detection
 * */
int32_t SubtreeManager::block_subtree(InodeNumber *p_inum)
{
    p_profiler->function_start();
    int32_t rc;
    log->debug_log("Starting to block inum %u.",*p_inum);
    p_profiler->function_sleep();
    pthread_mutex_lock(&mutex_wait_journal);
    p_profiler->function_wakeup();
    try
    {
        rc = set_blocked_root_inum(p_inum);
        if (!rc)
        {
            time_t timeout = time(NULL);
            
            map<pid_t, InodeNumber>::iterator it;
            bool journal_busy;
            while (1)
            {
                log->debug_log("Still waiting:");
                journal_busy=false;
                // set the absolute time by getting the current time + WRITE_BACK_INTERVALL
                
                for (it = threads_currently_working_on.begin(); it != threads_currently_working_on.end(); ++it)
                {
                    if ((it->second == *p_inum) && (it->first != getpid()))
                    {
                        journal_busy=true;
                        log->debug_log("Journal busy at inum %u.",*p_inum);
                    }
                }
                if (!journal_busy)
                {
                    log->debug_log("Journal finished work. Now blocked.");
                    rc = 0;
                    break;
                }
                if (timeout + 10 < time(NULL))
                {
                    log->error_log("Timeout reached. Unable to block journal.");
                    rc = -1;
                    break;
                }
                p_profiler->function_sleep();
                sleep(SUBTREEMANAGER_BLOCKING_SLEEP);
                p_profiler->function_wakeup();
            }        
            
        }
    }
    catch (SubtreeManagerException e)
    {
        log->error_log(e.what());
        free_blocked_root_inum();
        pthread_mutex_unlock(&mutex_wait_journal);
        p_profiler->function_end();
        throw e;
    }
    catch (...)
    {
        log->error_log("Unknown exception thrown.");
        free_blocked_root_inum();
        pthread_mutex_unlock(&mutex_wait_journal);
        p_profiler->function_end();
        throw SubtreeManagerException("Unknown exception thrown.");
    }    
    free_blocked_root_inum();
    pthread_mutex_unlock(&mutex_wait_journal);
    p_profiler->function_end();
    return rc;
    
} // end of SubtreeManager::block_subtree

/**
 * @brief Register an journal manager instance at the subtree manager
 * @param[in] p_jm pointer to the journal manager instance
 * @throw SubtreeManagerException
 * @return zero if this instance is registered, one if another instance
 * already got registered.
 * */
int32_t SubtreeManager::set_journal_manager(JournalManager *p_jm)
{
    p_profiler->function_start();
    int32_t rc=1;
    try
    {
        if (p_jm!=NULL)
        {
            // register journal managers pointer
            this->p_jm = p_jm;
            log->debug_log("Journal Manager instance registered.");
            rc=0;
        }
        else
        {
            log->error_log("Journal Manager pointer empty.");
        }
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Set_Journal_manager:Unknown exception thrown");
    }
    p_profiler->function_end();
    return rc;
} // end of SubtreeManager::set_journal_manager

/**
 * @brief Register an MltHandler instance at the subtree manager
 * @param[in] p_mlt_handler pointer to the mlt_handler instance
 * @throw SubtreeManagerException {details look at exception message}
 * @return zero if this instance is registered, one if another instance
 * already got registered.
 * */
int32_t SubtreeManager::set_mlt_handler(MltHandler *p_mlt_handler)
{
    p_profiler->function_start();
    int32_t rc=1;
    try
    {
        if (p_mlt_handler!=NULL)
        {
            // register mlt handler instance
            this->p_mlt_handler = p_mlt_handler;
            log->debug_log("MltHandler registered.");
            rc=0;
        }
        else
        {
            log->error_log("MltHandler pointer empty.");
        }
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Set_MltHandler:Unknown exception thrown");
    }
    p_profiler->function_end();
    return rc;
}

/**
 * @brief After the initialization the subtree manager knows whether it
 * is responsible for the root '/' object. The MDS asks the subtree
 * manager to handle delete operations on the root object.
 * @return True, if the root subtree '/' in the MLT is registered to this
 * server, false otherwise.
 * */
bool SubtreeManager::am_i_the_fs_root()
{
    return root_flag;
}

/** 
 * @brief Return inode number, if path represents the root path of a subtree
 * this mds is responsible for.
 * @param[in] path path of the subtree
 * @returns Root InodeNumber if path found, 0 otherwise
 */
InodeNumber SubtreeManager::get_inode_if_is_rootpath(std::string path)
{
    p_profiler->function_start();
    InodeNumber inum=0;
    set<InodeNumber>::iterator it;
    for (it=root_inode_numbers.begin(); it!=root_inode_numbers.end(); it++)
    {
        if (!path.compare(p_mlt_handler->get_path(*it)))
        {
            inum=*it;
            log->debug_log("Path %s found. Inode number is %u.",path.c_str(),inum);
            break;
        }
    }
    p_profiler->function_end();
    return inum;
}

/** 
 * @brief check whether the provided inode is root of an partition.
 * @param[in] p_inum pointer representing root inode number
 * @retuns true if p_inum is root of an subtree this mds is responsible for
 */
bool SubtreeManager::is_subtree_root_inode_number(InodeNumber *p_inum)
{
    p_profiler->function_start();
    set<InodeNumber>::iterator it;
    log->debug_log("Check inode number %u.",*p_inum);
    it = root_inode_numbers.find(*p_inum);
    if (it!=root_inode_numbers.end())
    {
        log->debug_log("Check inode number %u is a root inode.",*p_inum);
        p_profiler->function_end();
        return true;
    }
    log->debug_log("if val = %u",*it);
    p_profiler->function_end();
    return false;
}


/**
 * @brief start up all needed opbects and prepare for work
 * @throw SubtreeManagerException
 * @return 0 ok, nonzero represents and error.
 * */
int32_t SubtreeManager::run()
{
    p_profiler->function_start();
    int32_t rc=0;
    try
    {
        // check whether a valid journal manager instance is registered.
        rc= validate_journal_manager();
        if (rc)
        {
            log->error_log("Validate journal manager failed.");
            p_profiler->function_end();
            throw SubtreeManagerException("Journal manager failed.");
        }
        // check whether a valid mlt handler instance is registered.
        rc=validate_mlt_handler();
        if (rc)
        {
            log->error_log("Validate mlt_hander failed.");
            p_profiler->function_end();
            throw SubtreeManagerException("Validate mlt handler failed.");
        }
        // start initialization of subtrees
        rc = initialize_subtrees();
        if (rc)
        {
            log->error_log("Subtree initialization failed.");
            p_profiler->function_end();
            throw SubtreeManagerException("Initialization failed.");
        }
    }
    catch (JournalException e)
    {
        std::string s = "Journal exception cought";
        s+=e.get_msg().c_str(); 
        log->error_log(s.c_str());
        p_profiler->function_end();
        throw SubtreeManagerException(s.c_str());
    }
    catch (SubtreeManagerException e)
    {
        log->error_log("Subtreemanager exception cought: %s.",e.what());
        p_profiler->function_end();
        throw e;
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Run:Unknown exception thrown");
    }
    log->debug_log("Subtree manager startup completed.");
    set_status(started);
    p_profiler->function_end();
    return rc;
} // end of SubtreeManager::run


/** 
 * @brief Verify the sanity of the SubtreeManager and its used components
 * @throw SubtreeManagerException
 * @return 1 if verified, 0 if somethings wrong.
 * */
int16_t SubtreeManager::verify()
{
    p_profiler->function_start();
    int16_t rc=1;
    try
    {
        if (!AbstractMdsComponent::verify())
        {
            log->error_log("AbstractMdsComponent verify failed.");
            rc = 0;
        }
        if (get_status()!=started)
        {
            log->error_log("Invalid status detected.");
            rc = 0;
        }
        if (validate_mlt_handler()!=subtree_manager_global_ok)
        {
            log->error_log("MLT Handler failed verification.");
            rc = 0;
        }
        if (validate_journal_manager()!=subtree_manager_global_ok)
        {
            log->error_log("Journal manager failed verification.");
            rc = 0;
        }
        log->debug_log("return true.");
    }
    catch (...)
    {
        p_profiler->function_end();
        throw SubtreeManagerException("Some components are not ready.");
    }
    p_profiler->function_end();
    return rc;
}

/**
 * @brief perfom some checkts to see whether the mlt handler is
 * initialized and ready to run.
 * @throw SubtreeManagerException
 * @return 0 if ok, fail else.
 * */
int32_t SubtreeManager::validate_mlt_handler()
{
    p_profiler->function_start();
    // at the moment only checks whether the reference is set.
    int32_t rc=subtree_manager_global_ok;
    try
    {
        if (p_mlt_handler==NULL)
        {
            rc=subtree_manager_no_mlt_handler;
            log->error_log("Mlt handler instance not received.");
        }
    }
    catch(...)
    {
        log->error_log("Validate mlt handler failed.");
        p_profiler->function_end();
        throw SubtreeManagerException("Validate mlt handler failed");
    }
    log->debug_log("Validated rc:%d",rc);
    p_profiler->function_end();
    return rc;
} // end of SubtreeManager::validate_mlt_handler


/**
 * @brief perfom some checkts to see whether the journal manager is
 * initialized and ready to run.
 * @throw SubtreeManagerException
 * @return 0 if ok, fail else.
 * */
int32_t SubtreeManager::validate_journal_manager()
{
    p_profiler->function_start();
    // at the moment only checks whether the reference is set
    int32_t rc=subtree_manager_global_ok;
    try
    {
        if (p_jm==NULL)
        {
            rc=subtree_manager_no_journal_manager;
            log->error_log("No journal manager instance received.");
        }
    }
    catch(...)
    {
        log->error_log("Validate journal manager failed.");
        p_profiler->function_end();
        throw SubtreeManagerException("Validate journal manager failed");
    }
    log->debug_log("journal manager validated. rc=%d.",rc);
    p_profiler->function_end();
    return rc;
}

/** 
 * @brief Setter for block_root_inum variable
 * @throw SubtreeManagerException
 * @return 0 if ok, fail else.
 * */
int32_t SubtreeManager::set_blocked_root_inum(InodeNumber *p_inum)
{
    p_profiler->function_start();
    int32_t rc=-1;
    p_profiler->function_sleep();
    pthread_mutex_lock(&block_mutex);
    p_profiler->function_wakeup();
    try
    {
        log->debug_log("trying to set blocked_root_inum:%u.",*p_inum);
        if (blocked_root_inum == 0)
        {
            log->debug_log("blocked_root_inum set to %u.",*p_inum);
            blocked_root_inum = *p_inum;
            rc = 0;
        }
        else
        {
            log->warning_log("blocked_root_inum already set to %u. Cant set to %u.",blocked_root_inum,*p_inum);
        }
    }
    catch (...)
    {
        //pthread_mutex_unlock(&block_mutex);
        log->error_log("Error occured ");
        pthread_mutex_unlock(&block_mutex);
        p_profiler->function_end();
        throw SubtreeManagerException("Cant block root inum. Exception cought.");
    }
    log->warning_log("Failed to set blocked_root_inum to %u.",*p_inum);
    pthread_mutex_unlock(&block_mutex);
    p_profiler->function_end();
    return rc;
}

/** 
 * @brief Set block_root_inum variable to 0
 * @throw SubtreeManagerException
 * */
void SubtreeManager::free_blocked_root_inum()
{
    p_profiler->function_start();
    try
    {
        p_profiler->function_sleep();
        pthread_mutex_lock(&block_mutex);
        p_profiler->function_wakeup();
        blocked_root_inum = 0;
        pthread_mutex_unlock(&block_mutex);
        log->debug_log("blocked_root_inum freed.");
    }
    catch (...)
    {
        log->error_log("Exception thrown... but where");
        p_profiler->function_end();
        throw SubtreeManagerException("Cought an exception");
    }
    p_profiler->function_end();
}

/**
 * @brief Remenbers the journal instances every thread last accessed. 
 * This is necessary to identify busy journals in case of a conflicting 
 * migration.
 * @param[in] inum inode number of the root 
 * @throw SubtreeManagerException
 * @return 0 if ok, -1 on fail.
 * 
 * @todo else block, set using iterator?
 *  */
int16_t SubtreeManager::set_working_on(InodeNumber inum)
{
    p_profiler->function_start();
    int16_t rc = -1;
    try
    {
        // get pid of the current thread
        pid_t current = getpid();
        log->debug_log("Set working on %u to %u.",current, inum);
        map<pid_t,InodeNumber>::const_iterator it;
        if ((it=threads_currently_working_on.find(current)) == threads_currently_working_on.end())
        {
            // map current inode number to pid 
            threads_currently_working_on.insert( pair<pid_t,InodeNumber>( current, inum));
            rc = 0;
        }
        else
        {
            // update existing value
           threads_currently_working_on[current] = inum;
           rc = 0;
        } 
    }
    catch (...)
    {
        log->error_log("Exception cought.");
        p_profiler->function_end();
        throw SubtreeManagerException("Exception cought while set working on");
    }
    p_profiler->function_end();
    return rc;
}

/**
 * @brief Operation on the journal finished. Remove journal busy entry
 * @throw SubtreeManagerException {details look at exception message}
 * @return 0 if ok, -1 on fail.
 *  */
int16_t SubtreeManager::remove_working_on_flag()
{
    p_profiler->function_start();
    int16_t rc = -1;
    try
    {
        rc = set_working_on(0);
    }
    catch (...)
    {
        log->error_log("Remove working on threw an exception");
        p_profiler->function_end();
        throw SubtreeManagerException("Remove working on threw an exception");
    }
    p_profiler->function_end();
    return rc;
}

/**
 * @brief SubtreeManager constructor
 * @throw SubtreeManagerException
 * @param[in] basedir std::string type representing the base directory the systems working on
 * @param[in] conffile std::string type representing the config files path relative to the basedir
 * */
SubtreeManager::SubtreeManager(std::string basedir, std::string conffile): AbstractMdsComponent(basedir,conffile)
{
    // main constructor
    try
    {
        p_profiler = Pc2fsProfiler::get_instance();
        p_profiler->function_start();

        // initialized checks whether the mlt partitions were already read
        initialized = false;
        // root flag is set true, when this instance is responsible for the fs root object
        root_flag = false;
        blocked_root_inum=0;
        mutex_wait_journal = PTHREAD_MUTEX_INITIALIZER;
        block_mutex = PTHREAD_MUTEX_INITIALIZER;
        init_mutex = PTHREAD_MUTEX_INITIALIZER;
        // get export manager instance
    }
    catch(...)
    {
        log->error_log("Unknown exception thrown.");
        p_profiler->function_end();
        throw SubtreeManagerException("Subtreemanager constructor failed.");
    }
    p_profiler->function_end();
}   // end of constructor

/**
 * @brief Destructor
 * */
SubtreeManager::~SubtreeManager()
{
    // free mutex objects, delete log entry
    // TODO what about the export manager
    log->debug_log("subtree manager shutdown...");
    pthread_mutex_destroy(&block_mutex);
    pthread_mutex_destroy(&init_mutex);
    pthread_mutex_destroy(&mutex_wait_journal);
} // end of destructor


/** 
 * @brief Singleton SubtreeManager instance reference
 * */
SubtreeManager *SubtreeManager::p_instance=NULL;

/**
 * @brief Return the Singleton instance. Creates the instance if called
 * for the first time.
 * @param[in] basedir std::string type representing the base directory the systems working on
 * @return SubtreeManager instance reference
 * */SubtreeManager *SubtreeManager::get_instance(std::string basedir)
{
    if(p_instance == NULL)
    {
        // if no instance existing, create one
        std::string conf = string(SUBTREEMANAGER_CONF_FILE);
        //p_instance = new MetadataServer(p_jm, p_mlt_handler,basedir,conf);
        p_instance = new SubtreeManager(basedir,conf);
    }
    // return subtreemanager instance
    return p_instance;
} // end of SubtreeManagerget_instance
