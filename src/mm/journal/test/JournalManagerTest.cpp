#include "gtest/gtest.h"

#include <sstream>
#include <fstream>

#include "mm/journal/JournalManager.h"

namespace
{

#define FILE_NAME "operation.file"
#define DATA "testdata"
#define DATA_SIZE 9
#define PATH "."

class JournalManagerTest: public ::testing::Test
{
protected:

	JournalManagerTest()
	{
		inode_id = 2;
		parent_id = 1;
		char *device_identifier;
		device = new FileStorageDevice();
		device_identifier = strdup("/tmp");
		device->set_identifier(device_identifier);
		std::list<AbstractStorageDevice*> *devices = new std::list<
				AbstractStorageDevice*>();
		devices->push_back(device);
		ddf = new SimpleDataDistributionFunction();
		ddf->change_metadata_storage_devices(NULL, devices);
		ddm = new DataDistributionManager(ddf);

		sal = new StorageAbstractionLayer(ddm);
		inode_io = new EmbeddedInodeLookUp(sal, parent_id);

		jm = (JournalManager::get_instance());
		journal_1 = jm->create_journal(journal_id_1);
		journal_1->set_inode_io(inode_io);
		journal_1->set_sal(sal);
		journal_1->start();

		journal_2 = jm->create_journal(journal_id_2);
		journal_2->set_inode_io(inode_io);
		journal_2->set_sal(sal);
		journal_2->start();


	}

	virtual ~JournalManagerTest()
	{
		delete sal;
		delete ddm;
		delete ddf;
		delete device;

		jm->remove_journal(journal_id_1);
		jm->remove_journal(journal_id_2);
		journal_1 = NULL;
		journal_1 = NULL;
		jm = NULL;
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

	JournalManager* jm;
	InodeNumber journal_id_1;
	InodeNumber journal_id_2;
	Journal* journal_1;
	Journal* journal_2;

	InodeNumber parent_id;
	InodeNumber inode_id;
	inode_create_attributes_t i_create_att;
	EInode the_inode;
	char some_data[10];
};

TEST_F(JournalManagerTest, mds_get_journal_test)
{

	EXPECT_TRUE(jm->get_journal(journal_id_1) == journal_1);
	EXPECT_TRUE(jm->get_journal(journal_id_2) == journal_2);

}


TEST_F(JournalManagerTest, operate_on_journals)
{
	// start the journal manager
	int operations = 100;
	InodeNumber i;


	for(i = inode_id; i< inode_id+operations; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;

		EXPECT_TRUE(journal_1->handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	}
	sleep(2);

	int32_t bitfield;
	inode_update_attributes_t update_attr;
	for(i = inode_id; i< inode_id+operations; i++)
	{
		bitfield = 0;
		set_size(i, &bitfield, &update_attr);
		EXPECT_TRUE(journal_1->handle_mds_update_attributes_request(&i, &update_attr, &bitfield) == 0);
	}

	for(i = inode_id; i<inode_id +operations; i++)
	{
		EInode the_inode;
		the_inode.inode.size = 0;

		EXPECT_TRUE(journal_1->handle_mds_einode_request(&i, &the_inode) == 0);
		if(i != the_inode.inode.size)
			cout << i << ": " << the_inode.inode.size << endl;
		EXPECT_TRUE(the_inode.inode.size == i);
	}

	for(int i = inode_id; i<inode_id+operations; i++)
	{
		InodeNumber id = i;
		EXPECT_TRUE(journal_1->handle_mds_delete_einode_request(&id) == 0);
	}

	// stop the journal manager
	jm->remove_journal(journal_1->get_journal_id());
}


} // namespace
