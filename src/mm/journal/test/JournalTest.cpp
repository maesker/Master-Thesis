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

class JournalTest: public ::testing::Test
{
protected:

	JournalTest()
    {

	    inode_id = 2;
	    parent_id = 1;
	    char *device_identifier;
	    device_identifier = strdup("/tmp");

	    sal = new StorageAbstractionLayer(device_identifier);
	    inode_io = new EmbeddedInodeLookUp(sal, parent_id);


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

    virtual ~JournalTest()
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


TEST_F(JournalTest, get_path_test)
{
	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;

	Journal journal(1);
	InodeNumber root_node = 1;
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	string path;
	string a_name = "a";
	strcpy(the_inode.name, a_name.c_str());
	the_inode.inode.inode_number = inode_id;


	EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	EXPECT_TRUE(journal.get_path(inode_id, path) == 0);
	EXPECT_TRUE(path.compare(a_name) == 0);

	string b_name = "b";
	InodeNumber b_number = inode_id + 1;
	strcpy(the_inode.name, b_name.c_str());
	the_inode.inode.inode_number = b_number;

	EXPECT_TRUE(journal.handle_mds_create_einode_request(&inode_id, &the_inode) == 0);
	EXPECT_TRUE(journal.get_path(b_number, path) == 0);
	EXPECT_TRUE(path.compare("a/b") == 0);

	EXPECT_TRUE(journal.handle_mds_delete_einode_request(&inode_id) == 0);
	EXPECT_TRUE(journal.handle_mds_delete_einode_request(&b_number) == 0);

	journal.close_journal();
	journal.run_write_back();

}

TEST_F(JournalTest, rename_request_test)
{
	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;

	Journal journal(1);
	InodeNumber root_node = 1;
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	stringstream ss;
	ss << inode_id;
	strcpy(the_inode.name, ss.str().c_str());
	the_inode.inode.inode_number = inode_id;

	string new_name = "new_name";

	EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	EXPECT_FALSE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);

	EXPECT_TRUE(journal.handle_mds_move_einode_request(&parent_id, (FsObjectName*) new_name.c_str(), &parent_id, &(the_inode.name))
			== 0);

	InodeNumber result_inode_number;
	EXPECT_TRUE(journal.handle_mds_lookup_inode_number_by_name(&parent_id, (FsObjectName*) new_name.c_str(), &result_inode_number) == 0);
	EXPECT_TRUE(result_inode_number == inode_id);


	journal.close_journal();
	journal.run_write_back();

	Journal test_journal(1);
	test_journal.set_inode_io(inode_io);
	test_journal.set_sal(sal);


	test_journal.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);
	ASSERT_TRUE(rdir_return.nodes_len == 1);
	ASSERT_TRUE(rdir_return.dir_size == 1);
	ASSERT_TRUE(rdir_return.nodes[0].inode.inode_number == the_inode.inode.inode_number);
	ASSERT_TRUE(strcmp(rdir_return.nodes[0].name, new_name.c_str()) == 0);
	EXPECT_TRUE(test_journal.handle_mds_delete_einode_request(&inode_id) == 0);
	test_journal.close_journal();
	test_journal.run_write_back();


}

TEST_F(JournalTest, move_request_test_1)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;


	InodeNumber root_node = 1;

	InodeNumber a_number = 2;
	InodeNumber b_number = 3;

	EInode a;
	{
		stringstream ss;
		ss << "a";
		strcpy(a.name, ss.str().c_str());
		a.inode.inode_number = a_number;
	}

	EInode b;
	{
		stringstream ss;
		ss << "b";
		strcpy(b.name, ss.str().c_str());
		b.inode.inode_number = b_number;
	}

	EXPECT_TRUE(journal.handle_mds_readdir_request(&root_node, &offset, &rdir_return) == 0);
	ASSERT_TRUE(rdir_return.dir_size == 0);

	EXPECT_TRUE(journal.handle_mds_create_einode_request(&root_node, &a) == 0);
	EXPECT_TRUE(journal.handle_mds_create_einode_request(&root_node, &b) == 0);

	// move a into b
	EXPECT_TRUE(journal.handle_mds_move_einode_request(&b_number, &(a.name), &root_node, &(a.name)) == 0);

	offset = 0;
	EXPECT_TRUE(journal.handle_mds_readdir_request(&root_node, &offset, &rdir_return) == 0);
	ASSERT_TRUE(rdir_return.dir_size == 1);
	ASSERT_TRUE(rdir_return.nodes_len == 1);
	EXPECT_TRUE(rdir_return.nodes[0].inode.inode_number == b_number);

	EInode result_einode;
	InodeNumber result_number;
	EXPECT_TRUE(journal.handle_mds_einode_request(&b_number, &result_einode) == 0);
	EXPECT_TRUE(journal.handle_mds_lookup_inode_number_by_name(&b_number, &a.name, &result_number) == 0);
	EXPECT_TRUE(result_number == a_number);

	offset = 0;
	EXPECT_TRUE(journal.handle_mds_readdir_request(&b_number, &offset, &rdir_return) == 0);
	ASSERT_TRUE(rdir_return.dir_size == 1);
	ASSERT_TRUE(rdir_return.nodes_len == 1);
	EXPECT_TRUE(rdir_return.nodes[0].inode.inode_number == a_number);

}


TEST_F(JournalTest, move_request_test_2)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	InodeNumber root_node = 1;

	InodeNumber parent_1_id = 10;
	InodeNumber parent_2_id = 20;

	InodeNumber child_of_p_1_id = 11;
	InodeNumber child_of_p_2_id = 21;


	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;

	EInode child_1;
	{
		stringstream ss;
		ss << child_of_p_1_id;
		strcpy(child_1.name, ss.str().c_str());
		child_1.inode.inode_number = child_of_p_1_id;
	}

	EInode child_2;
	{
		stringstream ss;
		ss << child_of_p_2_id;
		strcpy(child_2.name, ss.str().c_str());
		child_2.inode.inode_number = child_of_p_2_id;
	}

	EXPECT_TRUE(journal.handle_mds_readdir_request(&root_node, &offset, &rdir_return) == 0);
	// put child 1 to parent 1
	EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_1_id, &child_1) == 0);
	EXPECT_FALSE(journal.handle_mds_create_einode_request(&parent_1_id, &child_1) == 0);

	// put child 2 to parent 2
	EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_2_id, &child_2) == 0);
	EXPECT_FALSE(journal.handle_mds_create_einode_request(&parent_2_id, &child_2) == 0);

	// move child 1 from parent 1 to parent 2
	EXPECT_TRUE(journal.handle_mds_move_einode_request(&parent_2_id, &(child_1.name), &parent_1_id, &(child_1.name)) == 0);

	InodeNumber result_number;
	EXPECT_TRUE(journal.handle_mds_lookup_inode_number_by_name(&parent_2_id, &(child_1.name), &result_number) == 0);
	EXPECT_TRUE(result_number == child_1.inode.inode_number);

	EXPECT_FALSE(journal.handle_mds_move_einode_request(&parent_2_id, &(child_1.name), &parent_1_id, &(child_1.name)) == 0 );


	journal.close_journal();
	journal.run_write_back();

	Journal test_journal(1);
	test_journal.set_inode_io(inode_io);
	test_journal.set_sal(sal);

	test_journal.handle_mds_readdir_request(&parent_2_id, &offset, &rdir_return);
	ASSERT_TRUE(rdir_return.dir_size == 2);
	ASSERT_TRUE(rdir_return.nodes_len == 2);


	EXPECT_TRUE(test_journal.handle_mds_lookup_inode_number_by_name(&parent_2_id, &(child_1.name), &result_number) == 0);
	EXPECT_TRUE(child_1.inode.inode_number == result_number);
	EXPECT_TRUE(test_journal.handle_mds_lookup_inode_number_by_name(&parent_2_id, &(child_2.name), &result_number) == 0);
	EXPECT_TRUE(child_2.inode.inode_number == result_number);
	EXPECT_TRUE(test_journal.handle_mds_delete_einode_request(&child_1.inode.inode_number) == 0);
	EXPECT_TRUE(test_journal.handle_mds_delete_einode_request(&child_2.inode.inode_number) == 0);
	test_journal.close_journal();
	test_journal.run_write_back();
}


TEST_F(JournalTest, simple_create_test)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	stringstream ss;
	ss << inode_id;
	strcpy(the_inode.name, ss.str().c_str());
	the_inode.inode.inode_number = inode_id;

	EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	EXPECT_FALSE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);


}

/**
 * Atomic create inode operation.
 */
TEST_F(JournalTest, mds_create_test)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	int max_nodes = 100;
	for(int i = inode_id; i< inode_id+max_nodes; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;
		EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	}
	journal.close_journal();
	journal.run_write_back();

	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;
	rdir_return.dir_size = 0;
	rdir_return.nodes_len = 0;
	journal.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);

	EXPECT_TRUE(rdir_return.dir_size == max_nodes);

	EInode e_inode;
	for(int i = inode_id; i<inode_id+max_nodes; i++)
	{
		EXPECT_NO_THROW(inode_io->get_inode(&e_inode, i));

		EXPECT_TRUE(e_inode.inode.size = i);

		EXPECT_NO_THROW(inode_io->delete_inode(i));
	}
}
//
//
///**
// * Atomic delete inode opeation.
// */

TEST_F(JournalTest, readdir_test)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	int inodes = 50;

	for(int i = inode_id; i< inode_id+inodes; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;

		EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	}

	cout << "readdir: " << endl;
	ReadDirReturn rdr;
	ReaddirOffset offset = 0;
	for (int k = 0; k < 5; k++)
	{
		journal.handle_mds_readdir_request(&parent_id, &offset, &rdr);
		cout << "dir_size: " << rdr.dir_size << endl;
		cout << "nodes_len: " << rdr.nodes_len << endl;

		for (unsigned int i = 0; i < rdr.nodes_len; i++)
		{
			cout << "inode id: " << rdr.nodes[i].inode.inode_number << "\t" << rdr.nodes[i].name << endl;
		}

		offset += 10;
	}

	for(unsigned int i = inode_id; i< inode_id+inodes; i++)
	{
		the_inode.inode.inode_number = i;
		EXPECT_TRUE(journal.handle_mds_delete_einode_request(&(the_inode.inode.inode_number)) == 0);
	}

	journal.close_journal();
	journal.run_write_back();
}
TEST_F(JournalTest, mds_delete_inode_test_1)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	for(int i = inode_id; i< inode_id+10; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;

		EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
		EXPECT_TRUE(journal.handle_mds_delete_einode_request(&(the_inode.inode.inode_number)) == 0);
	}
	journal.close_journal();
	journal.run_write_back();
}
//
///**
// * Atomic delete inode opeation.
// */
TEST_F(JournalTest, mds_delete_inode_test_2)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	for(int i = inode_id; i< inode_id+10; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;

		EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	}

	for(int i = inode_id; i<inode_id+10; i++)
	{
		InodeNumber id = i;
		EXPECT_TRUE(journal.handle_mds_delete_einode_request(&id) == 0);
	}
	journal.close_journal();
	journal.run_write_back();
}
//
///**
// * Atomic update inode operation.
// */
TEST_F(JournalTest, mds_update_attr_test)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	inode_update_attributes_t update_attr;
	int bitfield = 0;

	update_attr.size = 1000;
	SET_SIZE(bitfield);


	stringstream ss;
	ss << inode_id;
	strcpy(the_inode.name, ss.str().c_str());
	the_inode.inode.inode_number = inode_id;


	EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);

	bitfield = 0;
	time_t now = time(0);
	set_mtime(now, &bitfield, &update_attr);
	set_mode(S_IRWXU, &bitfield, &update_attr);
	EXPECT_TRUE(journal.handle_mds_update_attributes_request(&inode_id, &update_attr, &bitfield) == 0);

	bitfield = 0;
	set_atime(now, &bitfield, &update_attr);
	EXPECT_TRUE(journal.handle_mds_update_attributes_request(&inode_id, &update_attr, &bitfield) == 0);

	for(int i = 0; i< 10; i++)
	{

		bitfield = 0;
		set_size(i, &bitfield, &update_attr);
		EXPECT_TRUE(journal.handle_mds_update_attributes_request(&inode_id, &update_attr, &bitfield) == 0);
	}
	journal.close_journal();
	journal.run_write_back();


	EInode e_inode;
//	EXPECT_NO_THROW(inode_io->get_inode(&e_inode, inode_id));

	Journal j_2(1);
	j_2.set_inode_io(inode_io);
	j_2.set_sal(sal);

	InodeNumber parent_id = 1;
	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;
	j_2.handle_mds_readdir_request(&parent_id, &offset, &rdir_return);

	EInode e_inode_2;
	EXPECT_TRUE(j_2.handle_mds_einode_request(&inode_id, &e_inode_2) == 0);


	EXPECT_TRUE(journal.handle_mds_delete_einode_request(&inode_id) == 0);
	journal.close_journal();
	journal.run_write_back();
}

/**
 * Atomic create inode operation.
 */
TEST_F(JournalTest, distributed_create_test)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	uint64_t operation_id = 1;

	for(int i = inode_id; i< inode_id+10; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;
		operation_id = i;

		EXPECT_TRUE(journal.handle_distributed_create_inode(operation_id, parent_id, the_inode) == 0);
	}
	journal.close_journal();
	journal.run_write_back();

	EInode e_inode;
	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;
	rdir_return = inode_io->read_dir(parent_id, offset);
	rdir_return = inode_io->read_dir(parent_id, 10);

	for(int i = inode_id; i<inode_id+10; i++)
	{

		EXPECT_NO_THROW(inode_io->get_inode(&e_inode, i));

		EXPECT_NO_THROW(inode_io->delete_inode(i));
	}
}

TEST_F(JournalTest, distributed_delete_inode_test)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);
	for(int i = inode_id; i< inode_id+10; i++)
	{
		stringstream ss;
		ss << i;
		strcpy(the_inode.name, ss.str().c_str());


		the_inode.inode.inode_number = i;
		EXPECT_TRUE(journal.handle_mds_create_einode_request(&parent_id, &the_inode) == 0);
	}

	for(int i = inode_id; i<inode_id+10; i++)
	{
		InodeNumber id = i;


		EXPECT_TRUE(journal.handle_distributed_delete_inode(i, id) == 0);
	}
	journal.close_journal();
	journal.run_write_back();
}

TEST_F(JournalTest, simple_distributed_operation_test)
{
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	for(int i = 1; i<10; i++)
	{
		journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::Committed, some_data, 10);
	}
	journal.close_journal();
	journal.run_write_back();
}

TEST_F(JournalTest, distributed_operation_test)
{
	int operation_count = 10;
	int operation_id = inode_id;
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);


	for(InodeNumber i = inode_id; i< inode_id + operation_count; i++)
	{
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedStart, some_data, 10) == 0);
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedUpdate, some_data, 10) == 0);
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedResult, some_data, 10) == 0);

		stringstream ss;
		ss << operation_id;
		strcpy(the_inode.name, ss.str().c_str());
		the_inode.inode.inode_number = operation_id;

		EXPECT_TRUE(journal.handle_distributed_create_inode(i, parent_id, the_inode) == 0);
		operation_id++;

		EInode myEinode;
		EXPECT_TRUE(journal.handle_mds_einode_request( &i, &myEinode) == 0);
	}
	journal.close_journal();
	journal.run_write_back();

	ReaddirOffset offset = 0;
	ReadDirReturn rdir_return;

	do{
		rdir_return = inode_io->read_dir(parent_id, offset);
		offset += 10;
	}while(offset < rdir_return.dir_size);

	EInode e_inode;
	for(int i = inode_id; i<inode_id+operation_count; i++)
	{
		EXPECT_NO_THROW(inode_io->get_inode(&e_inode, i));

		EXPECT_NO_THROW(inode_io->delete_inode(i));
	}
}

TEST_F(JournalTest, extended_distributed_operation_test)
{
	int operation_count = 10;
	int operation_id = inode_id;
	Journal journal(1);
	journal.set_inode_io(inode_io);
	journal.set_sal(sal);

	for(InodeNumber i = inode_id; i< inode_id + operation_count; i++)
	{
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedStart, some_data, 10) == 0);
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedUpdate, some_data, 10) == 0);
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedResult, some_data, 10) == 0);
	}
	journal.close_journal();
	journal.run_write_back();
	for(InodeNumber i = inode_id; i< inode_id + operation_count; i++)
	{
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::Committed, some_data, 10) == 0);
	}
	journal.close_journal();
	journal.run_write_back();

	for(InodeNumber i = inode_id; i< inode_id + operation_count; i++)
	{
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedStart, some_data, 10) == 0);
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedUpdate, some_data, 10) == 0);
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::DistributedResult, some_data, 10) == 0);
	}
	journal.close_journal();
	journal.run_write_back();

	for(InodeNumber i = inode_id; i< inode_id + operation_count; i++)
	{
		EXPECT_TRUE(journal.add_distributed(i, Module::DistributedAtomicOp, OperationType::DistributedOp, OperationStatus::Committed, some_data, 10) == 0);
	}
	journal.close_journal();
	journal.run_write_back();

}

} // namespace
