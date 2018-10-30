/**
 * @file CommonJournalTypes.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 * @brief Defines some common journal data structures.
 */

#ifndef COMMONJOURNALTYPES_H_
#define COMMONJOURNALTYPES_H_

#include <EmbeddedInode.h>


/*
 * Defines the data structure for the move operation.
 * MoveData is used inside the journal to identify the old location of an einode.
 */
struct MoveData
{
	InodeNumber old_parent; /**< The old parent id before the move operation. */
	FsObjectName old_name; /**< The old name of the einode before the move operation. */
};



#define OPERATION_SIZE sizeof(MoveData)  /**< Defines the data array size of an operation. */
#define CHUNK_SIZE 50		/** < Defines the size of a chunk */
#define WRITE_BACK_INTERVALL 2 /**< Write back interval in seconds.*/
#define WRITE_BACK_SLEEP 10 /**< Sleeping time of the write back thread. */
#define CACHE_LIFETIME 600 /**< Cache lifetime without access. */
#define MAX_CACHE_SIZE 10000000 /**< The maximum cache size before emergency write back / garbage collections starts */

#define SMART_WRITE_BACK 0 /**< 1 Enables the smart write back process */



enum OperationStatus
{
	InvalidStatus = -1, /**< Not used yet */
	WrittenBack = 0, /**< enum Defines written back status of an operation. */
	Committed = 1, /**< enum Defines committed status of an operation. */
	NotCommitted = 2, /**< enum Defines not committed status of an operation. */
	Aborted = 3, /**< enum Defines an aborted operation. */
	DistributedStart = 4,
	DistributedUpdate = 5,
	DistributedResult = 6

};

enum CacheStatusType
{
	Present,
	NotPresent,
	Deleted
};

enum OperationType
{
	CreateINode, SetAttribute, DeleteINode, MoveInode, RenameObject, UnknownType, SubtreeMigration, DistributedOp
};

enum OperationMode
{
	Atomic, Disributed
};

enum Module
{
	DistributedAtomicOp, MDS, Dummy
};

/**
 * Defines the inode attributes which are holding by the journal.
 */
struct InodeAttributes
{
	InodeNumber inode_id;
	InodeNumber parent_id;
	time_t atime;
	time_t mtime;
	mode_t mode;
	size_t size;
	gid_t gid;
	uid_t uid;
	nlink_t st_nlink;
	int has_acl;
	int32_t bitfield;
};

/**
 * Defines the data structure of an operation.
 */
struct OperationData
{
	int32_t operation_number;
	OperationType operation_type;
	uint64_t operation_id; 				/** < Unique Identifier for every Operation object. */
	OperationStatus status; 			/** < current status of the operation. */
	OperationMode mode; 				/** < Indicates whether the operation is atomic or distributed. */
	Module module; 						/** < Indicates the owner of the operation. */
	char data[OPERATION_SIZE]; 			/** < Storing information about the operation. */
	EInode einode;

	InodeNumber parent_id;
	int32_t bitfield;
};

/**
 * Defines the data structure of a chunk.
 */
struct ChunkData
{
	int operations_count; /** < Number of operations. */
	OperationData operations[CHUNK_SIZE]; /**< List of pperations */
};

/*
 * Defines the mapping between an operation and a chunk.
 */
struct OperationMapping
{
	int32_t operation_number;
	int32_t chunk_number;
	OperationStatus status;
};

/**
 * Defines the mapping between an operation a a chunk.
 */
struct ModificationMapping
{
	OperationType type;
	int32_t chunk_number;
};

/**
 * Defines the access information for an cached object.
 */
struct AccessData
{
	InodeNumber inode_numer; /*< The identifier for the cached object. */
	timeval last_access;   /*< The last time as the object was accessed. */
	int32_t cache_size;	/*< The occupied size of the object. */
	bool dirty;	/*< Flag identifies whether the cached object is consistent with the storage or not. */
};


#endif /* COMMONJOURNALTYPES_H_ */
