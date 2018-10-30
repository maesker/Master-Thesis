#include "gtest/gtest.h"


#include <sstream>
#include <fstream>

#include "mm/journal/JournalRecovery.h"

namespace
{

#define DATA "testdata"
#define DATA_SIZE 9

class JournalRecoveryTest: public ::testing::Test
{
protected:

	JournalRecoveryTest()
    {
	    parent_id = 50000;
	    inode_id = parent_id + 1;

	    char *device_identifier;
	    device_identifier = strdup("/tmp");

	    sal = new StorageAbstractionLayer(device_identifier);
	    inode_io = new EmbeddedInodeLookUp(sal, parent_id);

	    parent_id = 50000;
	    inode_id = parent_id + 1;

	    i_create_att.size = 10;
	    i_create_att.uid = 1;
	    i_create_att.mode = S_IFREG;
	    i_create_att.gid = 2;
	    FsObjectName name = "FsObjectName";

	    the_inode.inode.size = 10;
	    the_inode.inode.uid = 1;
	    the_inode.inode.mode = S_IFREG;
	    the_inode.inode.gid = 2;

	    strcpy(some_data, "some_data");
    }

    virtual ~JournalRecoveryTest()
    {
    	delete sal;
    }


    void set_size(int size, int* bitfield, inode_update_attributes_t* u_attr)
    {
    	u_attr->size = size;
    	SET_SIZE(*bitfield);
    }

    void set_atime(int atime, int* bitfield, inode_update_attributes_t* u_attr)
    {
    	u_attr->atime = atime;
    	SET_ATIME(*bitfield);
    }

    void set_mtime(int mtime, int* bitfield, inode_update_attributes_t* u_attr)
    {
    	u_attr->mtime = mtime;
    	SET_MTIME(*bitfield);
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }


    StorageAbstractionLayer *sal;
    AbstractDataDistributionFunction *ddf;
    DataDistributionManager *ddm;
    AbstractStorageDevice *device;
    EmbeddedInodeLookUp* inode_io;

    InodeNumber parent_id;
    InodeNumber inode_id;
    inode_create_attributes_t i_create_att;
    EInode the_inode;
    char some_data[10];
};



/**
 * Atomic create inode operation.
 */
TEST_F(JournalRecoveryTest, recover_test)
{
	int operations = 10;
	Journal journal(parent_id);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	for(int i = inode_id; i< inode_id+operations; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;
		EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	}


	JournalRecovery jr(sal);
	vector<InodeNumber> partitions;
	partitions.push_back(parent_id);
	map<InodeNumber, Journal*> journals;

	jr.start_recovery(journals, partitions);
	Journal* recovered_journal = journals.at(parent_id);
	recovered_journal->set_sal(sal);
	recovered_journal->set_inode_io(inode_io);

	recovered_journal->start();

	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;
	recovered_journal->handle_mds_readdir_request(&parent_id, &offset, &rdir_return);

	EXPECT_TRUE(rdir_return.dir_size == operations);

	EInode e_inode;
	for(InodeNumber i = inode_id; i<inode_id+operations; i++)
	{
		EXPECT_NO_THROW(inode_io->get_inode(&e_inode, i));

		EXPECT_TRUE(e_inode.inode.size = i);

		EXPECT_NO_THROW(inode_io->delete_inode(i));
	}
	recovered_journal->stop();
}

} // namespace
