#include "gtest/gtest.h"


#include <sstream>
#include <fstream>

#include "mm/journal/CommonJournalTypes.h"
#include "mm/journal/Journal.h"
#include "mm/journal/WriteBackProvider.h"

namespace
{

#define FILE_NAME "operation.file"
#define DATA "testdata"
#define DATA_SIZE 9
#define PATH "."

#define INODE_SIZE 10


#define MAX_INODE 100

class JournalExtendedTest: public ::testing::Test
{
protected:

	JournalExtendedTest()
    {

	    inode_id = 2;
	    parent_id = 1;
	    char *device_identifier;
	    device = new FileStorageDevice();
	    device_identifier = strdup("/tmp");
	    device->set_identifier(device_identifier);
	    std::list<AbstractStorageDevice*> *devices = new std::list<AbstractStorageDevice*>();
	    devices->push_back(device);
	    ddf = new SimpleDataDistributionFunction();
	    ddf->change_metadata_storage_devices(NULL, devices);
	    ddm = new DataDistributionManager(ddf);

	    sal = new StorageAbstractionLayer(ddm);
	    inode_io = new EmbeddedInodeLookUp(sal, parent_id);


	    i_create_att.size = INODE_SIZE;
	    i_create_att.uid = 1;
	    i_create_att.mode = S_IFREG;
	    i_create_att.gid = 2;
	    FsObjectName name = "FsObjectName";

	    the_inode.inode.size = INODE_SIZE;
	    the_inode.inode.uid = 1;
	    the_inode.inode.mode = S_IFREG;
	    the_inode.inode.gid = 2;

	    strcpy(some_data, "some_data");
    }

    virtual ~JournalExtendedTest()
    {
    	delete sal;
    	delete ddm;
    	delete ddf;
    	delete device;
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

    void set_mode(int mode, int* bitfield, inode_update_attributes_t* u_attr)
    {
    	u_attr->mode = mode;
    	SET_MODE(*bitfield);
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




TEST_F(JournalExtendedTest, create_delete_create_delete)
{
	int max_inodes = 2000;
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	journal.start();


	map<InodeNumber, EInode> einode_map;
	map<InodeNumber, EInode>::iterator it;

	ReadDirReturn rdir_return;
	rdir_return.dir_size = 0;
	rdir_return.nodes_len = 0;

	ReaddirOffset offset = 0;

	the_inode.inode.inode_number = inode_id;
	EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);

	cout << "CREATE" << endl;

	journal.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);
	cout << "READDIR" << endl;
	ASSERT_TRUE(rdir_return.dir_size == 1);
	ASSERT_TRUE(rdir_return.nodes_len == 1);
	ASSERT_TRUE(rdir_return.nodes[0].inode.inode_number == the_inode.inode.inode_number);
	ASSERT_TRUE(rdir_return.nodes[0].inode.mode == the_inode.inode.mode);

	EXPECT_TRUE(journal.handle_mds_delete_einode_request(&inode_id) == 0);

	++inode_id;


	// create inodes
	for(int i = 0; i < max_inodes; i++)
	{
		InodeNumber inode_number = i + inode_id;
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());

		the_inode.inode.inode_number = inode_number;

		EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
		einode_map.insert(pair<InodeNumber, EInode>(inode_number, the_inode));
	}


	// read dir
	do
	{
		journal.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);
		for (int k = 0; k < rdir_return.nodes_len; k++)
		{
			EXPECT_TRUE(rdir_return.dir_size == max_inodes);

			it = einode_map.find(rdir_return.nodes[k].inode.inode_number);
			ASSERT_TRUE(it != einode_map.end());
			EXPECT_TRUE(it->second.inode.inode_number == rdir_return.nodes[k].inode.inode_number);
			EXPECT_TRUE(it->second.inode.mode == rdir_return.nodes[k].inode.mode);
			EXPECT_TRUE(it->second.inode.size == rdir_return.nodes[k].inode.size);
			einode_map.erase(it);

			offset++;
		}
	} while (offset < rdir_return.dir_size);


	// delete inodes
	for(int i = 0; i < max_inodes; i++)
	{
		InodeNumber inode_number = i + inode_id;
		EXPECT_TRUE(journal.handle_mds_delete_einode_request(&inode_number) == 0);
	}
	journal.stop();
}


TEST_F(JournalExtendedTest, create_delete_without_write_back)
{

	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	ReadDirReturn rdir_return;


	ReaddirOffset offset = 0;

	for(int i = 90; i< MAX_INODE; i++ )
	{
		map<InodeNumber, EInode> einode_map;
		map<InodeNumber, EInode>::iterator it;
		for(int j = 0; j < i; j++)
		{
			InodeNumber inode_number = inode_id + j;


			stringstream ss;
			ss << i;
			strcpy(the_inode.name, ss.str().c_str());

			the_inode.inode.inode_number = inode_number;

			EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
			einode_map.insert(pair<InodeNumber, EInode>(inode_number, the_inode));

			rdir_return.dir_size = 0;
			rdir_return.nodes_len = 0;
			offset = 0;

			do
			{
				journal.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);
				for(int k = 0; k < rdir_return.nodes_len; k++)
				{
					it = einode_map.find(rdir_return.nodes[k].inode.inode_number);
					EXPECT_TRUE(it != einode_map.end());

					EXPECT_TRUE(it->second.inode.inode_number == rdir_return.nodes[k].inode.inode_number);
					EXPECT_TRUE(it->second.inode.mode == rdir_return.nodes[k].inode.mode);
					EXPECT_TRUE(it->second.inode.size == rdir_return.nodes[k].inode.size);
					offset++;
				}
			}while(offset < rdir_return.dir_size);

//			inode_update_attributes_t update_attr;
//			update_attr.mode = 420;
//			int bitfield = 0;
//			SET_MODE(bitfield);
//
//			EXPECT_TRUE(journal.handle_mds_update_attributes_request(&inode_number, &update_attr, &bitfield));

		}


		rdir_return.dir_size = 0;
		rdir_return.nodes_len = 0;
		offset = 0;

		do
		{
			journal.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);
			for(int k = 0; k < rdir_return.nodes_len; k++)
			{
				it = einode_map.find(rdir_return.nodes[k].inode.inode_number);
				EXPECT_TRUE(it != einode_map.end());

				EXPECT_TRUE(it->second.inode.inode_number == rdir_return.nodes[k].inode.inode_number);
				EXPECT_TRUE(it->second.inode.mode == rdir_return.nodes[k].inode.mode);
				EXPECT_TRUE(it->second.inode.size == rdir_return.nodes[k].inode.size);

				einode_map.erase(it);
				offset++;
			}
		}while(offset < rdir_return.dir_size);

		EXPECT_TRUE(einode_map.empty());

		for(int j = 0; j < i; j++)
		{
			InodeNumber inode_number = inode_id + j;
			EXPECT_TRUE(journal.handle_mds_delete_einode_request(&inode_number) == 0);
		}
		offset = 0;
		rdir_return.dir_size = 0;
		rdir_return.nodes_len = 0;
		journal.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);

		EXPECT_TRUE(rdir_return.nodes_len == 0);
		EXPECT_TRUE(rdir_return.dir_size == 0);
		inode_id += MAX_INODE * 2;
	}

	journal.close_journal();
	journal.run_write_back();
}

} // namespace
