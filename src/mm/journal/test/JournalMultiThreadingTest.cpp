#include "gtest/gtest.h"

#include <sstream>
#include <fstream>
#include <pthread.h>

#include "mm/journal/JournalManager.h"

namespace
{

#define FILE_NAME "operation.file"
#define DATA "testdata"
#define DATA_SIZE 9
#define PATH "."

#define THREADS_NUM 10

class JournalMultiThreadingTest: public ::testing::Test
{
protected:

	JournalMultiThreadingTest()
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

		journal_id_1 = 1;
		journal_id_2 = 2;

		jm = (JournalManager::get_instance());
		jm->set_storage_abstraction_layer(sal);
		journal_1 = jm->create_journal(journal_id_1);
		journal_1->set_inode_io(inode_io);
		journal_1->set_sal(sal);
		journal_1->start();

//		journal_2 = jm->create_journal(journal_id_2);
//		journal_2->set_inode_io(inode_io);
//		journal_2->set_sal(sal);
//		journal_2->start();

		inode_id = 2;
		parent_id = 1;
	}

	virtual ~JournalMultiThreadingTest()
	{
		delete sal;
		delete ddm;
		delete ddf;
		delete device;

		jm->remove_journal(journal_id_1);
	//	jm->remove_journal(journal_id_2);
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

	static void* start_thread(void *ptr)
	{
		Helper* h = static_cast<Helper*> (ptr);
		h->jmt->run_thread(h->start_inode);
		return NULL;
	}

	void run_thread(InodeNumber start_inode)
	{
		EInode the_inode;
		InodeNumber inode_id = start_inode;
		InodeNumber i;





		int operations = intervall;

		for(i = inode_id; i< inode_id+operations; i++)
		{
			stringstream ss;
			ss << i;
			strcpy(the_inode.name, ss.str().c_str());

			the_inode.inode.inode_number = i;
			EXPECT_TRUE(journal_1->handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
		}


//		do
//		{
//			cout << "handle_mds_readdir_request offset: " << offset << endl;
//			journal_1->handle_mds_readdir_request(&parent_id, &offset, &rdir_return);
//			cout << "rdir.size: " << rdir_return.dir_size << "\tnodes_len" << rdir_return.nodes_len << endl;
//			for(int k = 0; k < rdir_return.nodes_len; k++)
//			{
//				it = einode_map.find(rdir_return.nodes[k].inode.inode_number);
//				if(it != einode_map.end())
//				{
//					einode_map.erase(it);
//				}
//				offset++;
//
//			}
//		}while(offset < rdir_return.dir_size);

		// EXPECT_TRUE(einode_map.empty());

		for(i = inode_id; i<inode_id+operations; i++)
		{
			InodeNumber id = i;
			EXPECT_TRUE(journal_1->handle_mds_delete_einode_request(&id) == 0);
		}
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

	char some_data[10];

	pthread_t threads[THREADS_NUM];

	int start_inode;
	int intervall;

	struct Helper
	{
		InodeNumber start_inode;
		JournalMultiThreadingTest* jmt;
	};
};



TEST_F(JournalMultiThreadingTest, operate_on_journals)
{
	intervall = 100;

	for(int i = 0; i<THREADS_NUM; i++)
	{
		Helper* helper = new Helper();
		helper->jmt = this;
		helper->start_inode = intervall * (i+inode_id);
		pthread_create(&(threads[i]), NULL, &JournalMultiThreadingTest::start_thread, helper);
	}

	for(int i = 0; i<THREADS_NUM; i++)
	{
		pthread_join(threads[i], NULL);
	}

	ReadDirReturn rdir_return;
	rdir_return.dir_size = 0;
	rdir_return.nodes_len = 0;
	ReaddirOffset offset = 0;
	journal_1->handle_mds_readdir_request(&parent_id, &offset, &rdir_return);
	cout << rdir_return.dir_size << endl;
	EXPECT_TRUE(rdir_return.dir_size == 0);

	jm->remove_journal(journal_id_1);
}


} // namespace
