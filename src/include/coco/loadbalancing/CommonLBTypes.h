/**
 * @file CommonLBTypes.h
 *
 * @date 6. Mai 2011
 * @author Denis Dridger
 *
 *
 * @brief This file defines some data types, which are commonly used by the loadbalancing managers
 */

#ifndef LOADBALANCINGTYPES_H_
#define LOADBALANCINGTYPES_H_

//for using string lists
#include <vector>
#include <string>
#include "global_types.h"
#include "coco/coordination/MltHandler.h"
#include "mm/storage/Partition.h"

using namespace std;

#define LB_MESSAGE_SIZE 128


//represents load of server 
struct ServerLoadMessage
{
	float cpu_load;
	uint32_t swaps;
	uint32_t threads;
	float normalized_load;
};

struct PossibleTargets
{
	Server server;
	ServerLoadMessage server_load;
};



//encodes possible messages that can arrive at the LBCommunicationHandler
enum MessageType {LOAD_REQUEST, PHYS_TREE_REMOVAL_REQUEST, UPDATE_PARTITION_OWNER, CREATE_NEW_PARTITION};

//simplest message, to be received by LBCommunicationHandler
struct LBMessage
{
	MessageType message_type;
	//char data[LB_MESSAGE_SIZE];
};


//encodes MLT update message, to be received by LBCommunicationHandler
struct MLTUpdate
{
	MessageType message_type;		
	InodeNumber inode;
	char server_address[32];
	int32_t server_port;
	char path[1024];
	
};


//encodes kind of partition change
enum SubtreeChange {FULL_SUBTREE_MOVE, PARTIAL_SUBTREE_MOVE};


//encodes subtree movement, to be received by DaoListener
struct DAOSubtreeMove
{	
	SubtreeChange subtree_change;
	InodeNumber inode;
	char receiver_address[32];
	char sender_address[32];
	char path[32];
	int32_t receiver_port;	
	
};

//needed to copy/remove a tree's physical data to/from disk
struct PhysicalTreeMigration
{
	Partition *partition;
	InodeNumber inode;
};

#endif /* LOADBALANCINGTYPES_H_ */
