/*
 * AdminOperations.cpp
 *
 * Sebastian Moors <mauser@smoors.de>
 *
 * This file provides operations which manage meta- and
 * dataservers.
 */

#include "AdminOperations.h"
#include "mlt/mlt.h"
#include "mlt/mlt_file_provider.h"
#include "coco/administration/AdminCommunicationHandler.h"
#include "global_types.h"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <iostream>
#include <vector>
#include <sstream>

//main configfile
#define MAIN_CONF "../conf/main.conf"

//for communication with MDS's load balancing module due to static tree move
#define STATIC_MOVE_PARAMS "/tmp/static_move_params"
#define STATIC_MOVE_FEEDBACK "/tmp/static_move_feedback"

/*
 * Helper functions
 */

/**
 * Splits up a C++ String into a vector
 * @param s The string which should be parsed
 * @param delim The delimiter
 * @param elems A Vector to hold the elements of the splitted string
 * @return A boolean status.
 */

//http://stackoverflow.com/questions/236129/how-to-split-a-string-in-c
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}


template <class T> bool from_string(T& t, const std::string& s, std::ios_base& (*f)(std::ios_base&))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}


/**
 * Converts an ip:port tuple to a server struct.
 * @param address The address of a server, represented by a ip:port tuple
 * @return The created server struct.
 */

asyn_tcp_server* address_to_server(char* address)
{
    std::string servername_port(address);
    std::string ip_address;
    int port;

    std::vector<std::string> split_vector = split( servername_port, ':');

    /*
     * The vector needs to have to elements, other wise either the port
     * or the address is missing
     */
    if( split_vector.size() == 2 )
    {
        if(! from_string<int>(port, split_vector[1], std::dec))
        {
           std::cout << "Failed to parse the portnumber." << std::endl;
        }
        ip_address = split_vector[0];
    } else {
        std::cout << "Malformed address: Should be server:port\n" << std::endl;
        return NULL;
    }

    return mlt_create_server( ip_address.c_str() , port );
}


/**
 * Checks whether given server address is in correct format
 * @param address The address of a server, represented by a ip:port tuple
 * @return true if address format ok 
 */
bool address_ok(char *address)
{
	asyn_tcp_server* local_server = address_to_server( address );
        if(local_server == NULL) return false;
	else return true;        
}


/**
 * Constructs server from ip address and port found in main configfile
 * @return ip and port of own server
 */
Server get_my_address()
{	
	ifstream input;    
	vector<string> lines;    
	string line;
	
	//open file, if file could not be opened an exception is thrown and not catched
	input.open(MAIN_CONF);
    
	if (input.fail())
    	{
        	try
        	{
            		input.close();
        	}
        	catch (exception e)
        	{
            		//do nothing
        	}
		
        	throw runtime_error("Error: cannot read main config file!");
	}  

	Server asyn_tcp_server;	
	int my_port = -1;
	char* my_address = NULL;
	
	string port("port");
	string address("address");
	
	//parse config file line by line and set the corresponding variables
  	while(getline(input, line))
	{
		//parse port settings
		if (line.compare(0, port.length(), port) == 0)
		{			
			my_port = atoi( line.substr(port.length()+1,line.length()).c_str() );							
		}

		//parse address		
		if (line.compare(0, address.length(), address) == 0)
		{			
			int size = line.substr(address.length()+1,line.length()).size();
			my_address = (char*) malloc (size+1);		
			memcpy((char*)my_address, line.substr(address.length()+1,line.length()).c_str(), size+1);									
		}
	}    

	//check whether all values could be set properly, abort execution if not

	if ( (my_port == -1) ) 
		throw runtime_error("Error while parsing main configuration file: Entry port' has bad entry or doesn't exist. Aborting...");
	
	if ( (my_address == NULL) )
		throw runtime_error("Error while parsing main configuration file: Entry 'address' has bad entry or doesn't exist. Aborting...");		

    	try
    	{
        	input.close();
        	input.clear();
    	}
    	catch (exception e)
    	{
        	//do nothing
    	}

	//make server and return it 
	asyn_tcp_server.port = my_port;
	asyn_tcp_server.address = string(my_address);
	return asyn_tcp_server;
}


/*
 * Dataserver operations
 */


/**
 * Adds a dataserver to the dataserver list and communicates this event to other servers.
 * @param address The address of a server, represented by a ip:port tuple
 * @return A boolean status.
 */

int AdminOperations::add_ds( char * address )
{
	printf("added mds %s \n", address);
	return 0;
}


/**
 * Deletes a dataserver to the dataserver list and communicates this event to other servers.
 * @param address The address of a server, represented by a ip:port tuple
 * @return A boolean status.
 */

int AdminOperations::delete_ds( char * address )
{
	printf("deleted mds %s \n", address);
	return 0;
}

/**
 * Tests if a dataserver is responding to requests.
 * @param address The address of a server, represented by a ip:port tuple
 * @return A boolean status.
 */

int AdminOperations::check_ds( char * address )
{
	printf("checked ds %s \n", address);
	return 0;
}

/*
 * Metadataserver operations
 */

/**
 * Initialise a new metadata server. Construct
 * a new mlt and store it on disk.
 * @param address The address of the server, represented by a ip:port tuple
 * @return A boolean status
 */
int AdminOperations::init_mds( char * address )
{
        asyn_tcp_server* local_server = address_to_server( address );

        if(local_server == NULL)
        {
            std::cout << "Could not parse address." << std::endl;
            return -1;
        } else {
            int server_number = 1;

            struct asyn_tcp_server** servers = (struct asyn_tcp_server**) malloc(sizeof(struct asyn_tcp_server) * server_number );

            servers[0] = local_server;

            struct server_list* s_list = (struct server_list*) malloc(sizeof(struct server_list));

            s_list->servers = servers;
            s_list->n = server_number;

            struct mlt* list = mlt_create_new_mlt(local_server, 1 , FS_ROOT_INODE_NUMBER);
            list->server_list = s_list;

            write_mlt(list, MLT_PATH);
            mlt_destroy_mlt(list);

            std::cout << "The mlt was written successfully to " << MLT_PATH << " ."<<  std::endl;
        }

        return 0;
}


/**
 * Adds a metadataserver to the metadataserver list and communicates this event to other servers.
 * @param address The address of a server, represented by a ip:port tuple
 * @return A boolean status.
 */
int AdminOperations::add_mds(char *address)
{		
	if (!address_ok(address)) 
	{
		printf("Could not parse address.\n");
		return -1;
	}
	else
	{

		//split string address (ip:port
		string servername_port(address);
		string ip_address;
    		vector<string> split_vector = split( servername_port, ':');
		ip_address = split_vector[0];
		unsigned short port = atoi(split_vector[1].c_str());
		
		//pointer to empty MLT
		MltHandler* mlt_handler = &(MltHandler::get_instance());

		//init MLT
		mlt_handler->read_from_file(MLT_PATH);

		//add specified server
		mlt_handler->add_server(ip_address, port);	
	
		//write mlt to file
		if (mlt_handler->write_to_file(MLT_PATH) == 0)
		{
			printf("Server added to local MLT! \n");
		
			this->list_mds();	

			printf("Use --synch-mlt to broadcast the changes!\n");		
			printf("Note that metadataServer has to be started before! \n");
		
		}
		else 
		{
			printf("Error: cannot write MLT file!");			
			return -1;
		}

	}
		return 0;	 	
}

/**
 * Removes a metadataserver to the metadataserver list and communicates this event to other servers.
 * @param address The address of a server, represented by a ip:port tuple
 * @return A boolean status.
 */
int AdminOperations::remove_mds(char *address)
{		
	if (!address_ok(address)) 
	{
		printf("Could not parse address.\n");
		return -1;
	}
	else
	{
		//split string address (ip:port
		string servername_port(address);
		string ip_address;
    		vector<string> split_vector = split( servername_port, ':');
		ip_address = split_vector[0];
		unsigned short port = atoi(split_vector[1].c_str());
		
		//pointer to empty MLT
		MltHandler* mlt_handler = &(MltHandler::get_instance());

		//init MLT
		mlt_handler->read_from_file(MLT_PATH);

		//add specified server
		if (mlt_handler->remove_server(ip_address, port) != 0)
		{
			printf("Error: cannot remove server %s:%i \n", ip_address.c_str(),port);
			return -1;
		}	
	
		//write mlt to file
		if (mlt_handler->write_to_file(MLT_PATH) == 0)
		{
			printf("Server removed from local MLT! \n");
		
			this->list_mds();

			printf("Use --synch-mlt to broadcast the changes!\n");		
			printf("Note that metadataServer has to be started before! \n");
		
		}
		else 
		{
			printf("Error: cannot write MLT file!");		
			return -1;
		}

	}
	return 0;	 	
}

/**
 * List servers found in the MLT
 */
void AdminOperations::list_mds()
{		
	//pointer to empty MLT
	MltHandler* mlt_handler = &(MltHandler::get_instance());

	//init MLT
	mlt_handler->read_from_file(MLT_PATH);		
		
	vector <Server> server_list;
	mlt_handler->get_server_list(server_list);
	int servers = server_list.size();
	
	printf("Servers in MLT: \n");

	for (int k = 0; k < servers; k++) 
		printf("%s \n", server_list.at(k).address.c_str());		
	
}
 
/**
 * Broadcasts own MLT to all servers found in the MLT
 * @return A boolean status.
 */
int AdminOperations::synch_mlt()
{				
	//pointer to empty MLT
	MltHandler* mlt_handler = &(MltHandler::get_instance());

	//init MLT
	mlt_handler->read_from_file(MLT_PATH);

	mlt_handler->write_to_file("/tmp/temp_mlt");

	vector <Server> server_list;
	mlt_handler->get_server_list(server_list);
	int servers = server_list.size();

	
	printf("Sent MLT update request to servers: \n");
	for (int k = 0; k < servers; k++) 
	{	
		printf("%s \n", server_list.at(k).address.c_str());
	}	
		
	//finish if temp mlt was deleted by AdminCommunicationHandler or finish after timeout of 10 secs
	int j = 0;
	while(1)
	{		
		if (FILE * fp = fopen("/tmp/temp_mlt", "rb"))
		{
			sleep(1);
			j++;
			if (j == 10) break;
		}
		else break;
	}	
	
	return 0;	 	
}



/**
 * Tests if a metadataserver is responding to requests.
 * @param address The address of a server, represented by a ip:port tuple
 * @return A boolean status.
 */

int AdminOperations::check_mds( char * address )
{
	printf("checked mds %s \n", address);
	return 0;
}

/**
 * List partitions (i.e. paths) managed by this MDS
 */
void AdminOperations::list_partitions()
{
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	
	//init MLT
	mlt_handler->read_from_file(MLT_PATH);

	//look into main config file and set my address	
	mlt_handler->set_my_address(get_my_address());
	
	//get own partitions from MLT
	vector<InodeNumber> partitions;
	mlt_handler->get_my_partitions(partitions);

	//print the paths
	printf("\n");
	printf("Partitions managed by MDS %s: \n", mlt_handler->get_my_address().address.c_str());		
	
	if(partitions.size() == 0)  printf("None \n");

	for(int k = 0; k < partitions.size(); k++)		
		printf("%s \n",(mlt_handler->get_path(partitions.at(k))).c_str());	

	printf("\n");	
}

/**
 * Adds a metadataserver to the metadataserver list and communicates this event to other servers.
 * @param address The address of a server, represented by a ip:port tuple
 * @return A boolean status.
 */
int AdminOperations::add_partition(char *asyn_tcp_server, char *path, char *inodenum)
{		
	if (!address_ok(asyn_tcp_server)) 
	{
		printf("Could not parse address.\n");
		return -1;
	}
	else
	{
		//split string address (ip:port
		string servername_port(asyn_tcp_server);
		string ip_address;
    		vector<string> split_vector = split( servername_port, ':');
		ip_address = split_vector[0];
		unsigned short port = atoi(split_vector[1].c_str());
    		Server mds;
    		mds.address = ip_address;
    		mds.port = port;
    
    		InodeNumber inum = atoi(inodenum);
		
		//pointer to empty MLT
		MltHandler* mlt_handler = &(MltHandler::get_instance());
		
		//init MLT
		mlt_handler->read_from_file(MLT_PATH);

		//look into main config file and set my address	
		mlt_handler->set_my_address(get_my_address());

		//add specified partition
    		mlt_handler->handle_add_new_entry(mds, 0, inum, path);
	
		//write mlt to file
		if (mlt_handler->write_to_file(MLT_PATH) == 0)
		{
			printf("Added new partition to MLT\n");		
			this->list_partitions();			
			printf("Use --synch-mlt to broadcast the changes!\n");		
			printf("Note that metadataServer has to be started before! \n");
		
		}
		else 
		{
			printf("Error: cannot write MLT file!");			
			return -1;
		}
	}
	
	return 0;	 	
}

/**
 * Invokes manual subtree move
 * @param address The address of a server that shall take the subtree, represented by a ip:port tuple
* @param path File system tree represented by the path
 * @return A boolean status.
 */
int AdminOperations::move_subtree(char *address, char *path)
{	
	//make sure no such file exists initially	
	remove(STATIC_MOVE_FEEDBACK);	

	if (!address_ok(address)) 
	{
		printf("Could not parse address.\n");
		return -1;
	}
	else 
	{
		if (FILE *afile = fopen(STATIC_MOVE_PARAMS, "w")) 
		{
			//store ip, port and path to file		
			string servername_port(address);		
    			vector<string> split_vector = split( servername_port, ':');		
			fprintf(afile, split_vector[0].c_str());	
			fprintf(afile, "\n");													
			fprintf(afile, split_vector[1].c_str());												
			fprintf(afile, "\n");		
			fprintf(afile, path);																	 
			fclose(afile);
			
			printf("Trying to move %s ...\n", path);
		
			int seconds = 0;

			while(1)
			{		
				sleep(1);	
				seconds++;		
		
				if(FILE * fp = fopen(STATIC_MOVE_FEEDBACK, "r")) 
				{			
					//read feedback from file
					char *feedback = new char[128]; fgets(feedback, 128, fp);									
					fclose(fp);

					//continue waiting if the file already exists but no content is there yet
					if(strcmp(feedback, "") == 0) continue; 
						
					if(strcmp(feedback,"MOVING_TREE_SUCCESSFUL") == 0)
					{
						printf("Done! \n");
						delete [] feedback ;
						return 0;
					}
					else
					{
						printf("Error moving tree. Load balancer returned following error code: \n");	
						printf("%s \n",feedback);
						delete [] feedback ;
						return -1;
					}													
				}
				
				if (seconds == 10) printf("It takes quite long. Perhaps something crashed... \n");
				
			}//while

		}//STATIC_MOVE_PARAMS exist

		else
		{
			printf("Cannot create file %s\n", STATIC_MOVE_PARAMS);
			return -1;
		}
	
	}//1st else
	return 0;
}


