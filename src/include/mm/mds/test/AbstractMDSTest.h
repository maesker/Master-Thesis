#ifndef ABSTRACT_MDS_TEST_H_
#define ABSTRACT_MDS_TEST_H_

/** 
 * @file  AbstractMDSTest.h
 * @class AbstractMDSTest
 * @author Markus Mäsker, maesker@gmx.net
 * @date  24.08.11
 * 
 * @brief Abstract test class implementing basic functionality to set up a mds test environment
 * */

#include "mm/mds/MetadataServer.h"
#include "mm/mds/MockupObjects.h"
#include "mm/mds/mds_sys_tools.h"
#include "mm/mds/test/ZmqFsalHelper.h"
#include "mm/mds/test/Validator.h"
#include "ReadDirReturn.h"

#include "gtest/gtest.h"
#include <sstream>
#include <fstream>
#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <stdlib.h>
#include <stack>


/** 
 * @def BASE_ENVIRONMENT
 * @brief location where the single test environments are stored */
#define BASE_ENVIRONMENT "mm/mds/test/environments/"

/** 
 * @def BASE_TEMP
 * @brief The basic working directory where the single tests are preformed*/
#define BASE_TEMP "/tmp/pc2fs/"

/** 
 * @def CMD_BACKUP_MLT 
 * @brief do a backup of the current mlt file*/
#define CMD_BACKUP_MLT  "mv /etc/mlt /etc/mlt_tmp"

/** 
 * @def CMD_RESTORE_MLT
 * @brief restore mlt backup
 * @todo add mlt_path define here*/
#define CMD_RESTORE_MLT "mv /etc/mlt_tmp /etc/mlt"

//#define RESULT_ARCHIVE "/tmp/pc2fs_archive/"

//class AbstractMDSTest: public ::testing::TestWithParam<const char*> 
/** 
 * @class AbstractMDSTest
 * @brief Abstract mds test class implementation. Creating a mds test environment
 * 
 * The AbstractMDSTest class creates:
 * @verbatim
 * - aLogger instance
 * - a ConfigurationManager instance
 * - a complete Storage Backend
 * - a JournalManager instance
 * - a MDS instance with SubtreeManager and MessageRouter instances
 * @endverbatim
 *
 * At first the 
 * */
class AbstractMDSTest: public ::testing::Test
{
    protected:
        AbstractStorageDevice *device;
        AbstractDataDistributionFunction *ddf;
        DataDistributionManager *ddm;
        StorageAbstractionLayer *storage_abstraction_layer;
        
        ConfigurationManager *cm;
        MetadataServer *mds;
        ZmqFsalHelper *p_zmqhelper;
        MltHandler *p_mlt_handler;
        JournalManager *jm;
        Logger *log;
        
        int archived;
        int startup_failure;
        std::string workdir;
        std::string test_environment_dir;
        std::string objname;
        std::string confdest;
            
        struct mallinfo mi_start;
        struct mallinfo mi_end;
        struct mallinfo mi_intermediate_start;
        struct mallinfo mi_intermediate_end;
        void init_memleak_check_end();
        void init_memleak_check_start();
        void intermediate_memleak_check_end();
        void intermediate_memleak_check_start();

        virtual void SetUp();
        virtual void TearDown();
        
    public:
        // constructor, destructor
        explicit AbstractMDSTest();
        ~AbstractMDSTest();
        
                
        void MDSTEST_create_file(
            InodeNumber partition_root_inode_number,
            InodeNumber parent_inode_number,
            InodeNumber *p_result);

        void MDSTEST_read_einode( 
            EInode *p_ei,
            InodeNumber partition_root_inode_number,
            InodeNumber inode_number);

        void MDSTEST_update(
            InodeNumber partition_root_inode_number,
            InodeNumber new_file,
            inode_update_attributes_t *attrs,
            int attribute_bitfield);

        void MDSTEST_readdir(
            InodeNumber partition_root_inum,
            InodeNumber parent_inum,
            ReaddirOffset offset,
            ReadDirReturn *p_rdir);
            
        void MDSTEST_mkdir(
            InodeNumber parition_root,
            InodeNumber parent,
            InodeNumber *p_result);

        void MDSTEST_parent_hierarchy(
            InodeNumber partition_root_inode_number,
            InodeNumber inode_number,
            inode_number_list_t *p_inode_number_list);

        void MDSTEST_lookup_by_name(
            InodeNumber partition_root_inode_number,
            InodeNumber parent_inode_number,
            FsObjectName *p_name,
            InodeNumber *p_result);

        void MDSTEST_full_create_file(
            InodeNumber partition_root_inode_number,
            InodeNumber parent_inode_number,
            FsObjectName *p_name,
            InodeNumber *p_result);
    
    private:
        void archive_result();
        void init_testenvironment();
};



#endif
