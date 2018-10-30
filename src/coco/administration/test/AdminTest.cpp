
#include <string>
#include <string.h>
#include <sstream>
#include <stdio.h>
#include "gtest/gtest.h"
#include "coco/administration/AdminCommunicationHandler.h"
#include "coco/coordination/MltHandler.h"


#define OBJECT_NAME "AdminTest"
#define MLT_FILE "mlt.dat2"


void* start_rec_srv(void* pntr)
{
	RecCommunicationServer::get_instance().receive();
}

AdminCommunicationHandler *com_handler;

void start_com_handler()
{
	AdminCommunicationHandler::create_instance();	
	com_handler = AdminCommunicationHandler::get_instance();
	
	com_handler->init(COM_AdminOp);
	com_handler->start();

	pthread_t thread1;
	pthread_create(&thread1, NULL, start_rec_srv, NULL);
}


class AdminTest : public ::testing::Test
{
	protected:
	    AdminTest()
	    {
	    }

	    ~AdminTest()
	    {
	    }


	    virtual void SetUp()
	    {
			
	    }

	    virtual void TearDown()
	    {
	    }

};


//helper function to convert ip:port to ip,port (not needed anymore if register_new_server code is integrated into AdminOperations.cpp)
std::vector<std::string> &split_test(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
std::vector<std::string> split_test(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return split_test(s, delim, elems);
}


//sumulates addition of a new mds to the cluster
//new mds has the address: 'sender_address', some known mds has the address 'receiver_address'
TEST (administration, register_new_server)
{
	int succeeded = 1;

	char *sender_address = "127.0.0.1:1111";
	char *receiver_address = "127.0.0.1:2222";	   
    
	//extract receivers ip address (port not needed here since staticaly defined in communication module)
	string servername_port(receiver_address);
	string ip_address;
    	vector<string> split_test_vector = split_test( servername_port, ':');
	ip_address = split_test_vector[0];
		
	//pointer to empty MLT
	MltHandler* mlt_handler = &(MltHandler::get_instance());		
	
	/* prepare message to be sent to some_server. message consists of type and data 
	* type: SERVER_ADDITION_GET_MLT_SIZE indicates that the server some_server shall return MLT's size
	* after it added the address of the new server.
	* data: the address of the new server, that shall be put into MLT of the receiving server */
	AdminMessage msg;
	msg.message_type = SERVER_ADDITION_GET_MLT_SIZE;
	memcpy(&msg.data, sender_address, 32);
	
	//need a string list because request_reply() requires one
	vector <string> receiver_list;
	receiver_list.push_back(ip_address);

	//some message id to be used internal by the communication module
	uint64_t message_id;

	//message list that shall store the message sent by the other server
	vector<ExtractedMessage> message_list;
		
	//send address of the new server and request the size of resulting MLT
	int ret = com_handler->request_reply(&message_id, (void*) &msg, sizeof(msg), COM_AdminOp, receiver_list, message_list);

	//check whether sending message worked properly
	if(ret != 0)
	{
		succeeded = 2;
	}

	//check whether server did reply to request
	if ( (com_handler->receive_reply(message_id, 1000) != 0))
	{
		succeeded = 3;
	}

	//receive size of MLT
	int32_t mlt_size;
	memcpy(&mlt_size, (int32_t*) message_list.at(0).data, sizeof(int32_t));

	//now get the actual data sending another message to the same server
	msg.message_type = SERVER_ADDITION_GET_MLT_DATA;
	
	//new message id and message list needed
	uint64_t message_id2;
	vector<ExtractedMessage> message_list2;
		
	//request MLT content
	int ret2 = com_handler->request_reply(&message_id2, (void*) &msg, sizeof(msg), COM_AdminOp, receiver_list, message_list2);

	//check whether sending message worked properly
	if(ret != 0)
	{
		succeeded = 4;
	}

	//check whether server did reply to request
	if ( (com_handler->receive_reply(message_id2, 1000) != 0))
	{
		succeeded = 5;
	}
 	
	//receive MLT data and store it to stream of previously determined size
	char *file_stream;
	file_stream = (char*) malloc(mlt_size);	
	memcpy(file_stream, (char*) message_list2.at(0).data, mlt_size);
	
	//create MLT file on disk
	FILE *mlt_file;	
	mlt_file = fopen(MLT_FILE, "wb");

	if (mlt_file == NULL)
	{
		cout << "Cannot create temporary MLT file" << endl;		
	}
	else
	{	
		fwrite (file_stream, 1, mlt_size, mlt_file);			
	}

	fclose(mlt_file);

	//recreate MLT handler (otherwise inmemory MLT is locked)
	mlt_handler->destroy_mlt();
	mlt_handler = &(MltHandler::get_instance());	
	
	//update the empty inmemory MLT with the data from disk
	mlt_handler->read_from_file(MLT_FILE);

	//test output
	vector <Server> server_list;	
	mlt_handler->get_server_list(server_list);

	if (  (server_list.at(0).address != "127.0.0.1") || (server_list.at(0).port != 1337))
		succeeded = 6;

	if (  (server_list.at(1).address != "128.0.0.1") || (server_list.at(1).port != 1338))
		succeeded = 7;
	
	if (  (server_list.at(2).address != "129.0.0.1") || (server_list.at(2).port != 1339))
		succeeded = 8;

	
	//send new address to all other servers 
	vector <string> server_list_only_ips;
	
	//grab only the ips from server_list, since request_reply() expects a string list of ips
	//TODO filter own ip
	for (int j = 0; j < server_list.size(); j++)	
		server_list_only_ips.push_back(server_list[j].address);			
				
	uint64_t message_id3;
	vector<ExtractedMessage> message_list3;
	
	AdminMessage msg3;
	msg3.message_type = SERVER_ADDITION_CONFIRM; 			
				
	com_handler->request_reply(&message_id2, (void*) &msg3, sizeof(msg), COM_AdminOp, server_list_only_ips, message_list3);
			
	//check whether servers did reply to request
	if ( (com_handler->receive_reply(message_id, 1000) != 0))
	{
		int32_t *servers;		
		int error = 0; 

		for (int i = 0; i < message_list3.size(); i++)
		{
			servers = (int32_t*) message_list3.at(i).data;

			if (*servers != server_list.size())
			{
				cout << "At least one server could not register this server" << endl;
				error = 1;
				break;
			}
		}
		
		if (error == 0) cout << "Server registration went successfully!" << endl;
		
	}
	else cout << "Error: other servers failed to confirm registration." << endl;


	cout << "test result of administration.register_new_server:" << endl;
	cout << succeeded << endl;

	//delete com_handler;

	EXPECT_EQ(1, succeeded);

}


int main(int argc, char **argv)
{
	start_com_handler();
	
	//create and init MLT
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1);
	mlt_handler->add_server("128.0.0.1", 1338);
	mlt_handler->add_server("129.0.0.1", 1339);
	
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


