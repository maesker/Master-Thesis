#ifndef SUBTREEMANAGER_H
#define SUBTREEMANAGER_H

/** 
 * @file SubtreeManager.h
 * @author Markus Mäsker, maesker@gmx.net
 * */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <map>
#include <unistd.h>

#include "exceptions/MDSException.h"
#include "mm/mds/AbstractMdsComponent.h"
#include "mm/journal/JournalManager.h"
#include "mm/journal/JournalException.h"
#include "mm/journal/Journal.h"

#include "global_types.h"
#include "coco/coordination/MltHandler.h"
#include "coco/coordination/CommonCoodinationTypes.h"
/*
#ifdef __cplusplus
extern "C"
{
#endif
#include "mlt/mlt.h"
#include "mlt/mlt_file_provider.h"
#ifdef __cplusplus
}
#endif
*/

#define SUBTREEMANAGER_CONF_FILE DEFAULT_CONF_DIR "/" "subtreemanager.conf"
#define SUBTREEMANAGER_BLOCKING_SLEEP 0.01

class SubtreeManager: public AbstractMdsComponent
{
public:
    ~SubtreeManager();
    int32_t run();
    static SubtreeManager *get_instance(std::string basedir);
    Journal* get_responsible_journal(InodeNumber *inum);

    int32_t set_journal_manager(JournalManager *p_jm);
    int32_t set_mlt_handler(MltHandler *p_mlt_handler);

    int32_t create_journal_instance(InodeNumber *p_inum);
    int32_t create_new_subtree(InodeNumber *p_inum, std::string *p_path);    
    
    InodeNumber get_inode_if_is_rootpath(std::string path);
    bool is_subtree_root_inode_number(InodeNumber *p_inum);
    bool am_i_the_fs_root();
    int32_t block_subtree(InodeNumber *inum);
    void free_blocked_root_inum();
    
    int16_t investigate_subtree_merge_potential();
    int32_t handle_migration_create_partition(InodeNumber *p_inum);
    int32_t handle_migration_quit_partition(InodeNumber *p_inum);
    int16_t remove_working_on_flag();
    int16_t verify();

private:
    explicit SubtreeManager(std::string basedir, std::string conffile);
    
    float JOURNAL_CHECK_INTERVALL;
    mutable pthread_mutex_t block_mutex;
    mutable pthread_mutex_t init_mutex;
    mutable pthread_mutex_t mutex_wait_journal;
    pthread_cond_t cond_executed;
    
    bool initialized;
    bool root_flag;
    InodeNumber blocked_root_inum;
    
    map<pid_t , InodeNumber> threads_currently_working_on;
    set<InodeNumber> root_inode_numbers;
    
    static SubtreeManager *p_instance;

    JournalManager *p_jm;
    MltHandler *p_mlt_handler;

    int32_t initialize_subtrees();
    int32_t validate_mlt_handler();
    int32_t validate_journal_manager();
    int32_t initialize_new_subtree(InodeNumber root);
    
    int32_t set_blocked_root_inum(InodeNumber *p_inum);
    int16_t set_working_on(InodeNumber inum);
    
    void investigate_subtree_merge_potential_worker(InodeNumber *p_parent, int16_t *flag);
    int32_t merge_two_subtrees(InodeNumber *p_parent_inum, InodeNumber *p_subdir);
};

/** 
 * @enum SubtreeManagerErrorCodes
 * @brief SubtreeManager function error codes
 * */
enum SubtreeManagerErrorCodes
{
    subtree_manager_global_ok,
    subtree_manager_initialization_failed,
    subtree_manager_startup_failed,
    subtree_manager_own_server_address_not_specified,
    //subtree_manager_startup_failed,
    subtree_manager_no_mlt_handler,
    subtree_manager_no_journal_manager,
    subtree_manager_error,
    subtree_manager_no_inode_number_provided,
    subtree_manager_no_journal_error,
    subtree_manager_new_subtree_create_error,
    subtree_manager_merge_error,
};

#endif
