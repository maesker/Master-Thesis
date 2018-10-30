//our own includes..
//
#include <stdarg.h>

#include "zmq.h"
#include "fsal_zmq_client_library.h"
#include "fsal_zmq_mds_library.h"
#include "message_types.h"
#include "global_types.h"
#include "communication.h"
#include <stdlib.h>

const size_t size_fsobject_name = sizeof(FsObjectName);
const size_t size_fsal_inode_number_hierarchy_request = sizeof(struct FsalInodeNumberHierarchyRequest);
const size_t size_fsal_parent_inode_number_lookup_request = sizeof(struct FsalParentInodeNumberLookupResponse);
const size_t size_fsal_lookup_inode_number_by_name_request = sizeof(struct FsalLookupInodeNumberByNameRequest);
const size_t size_fsal_einode_request = sizeof(struct FsalEInodeRequest);
const size_t size_fsal_readdir_request = sizeof(struct FsalReaddirRequest);
const size_t size_fsal_create_einode_request = sizeof(struct FsalCreateEInodeRequest);
const size_t size_fsal_delete_inode_request = sizeof(struct FsalDeleteInodeRequest);
const size_t size_fsal_update_attributes_request = sizeof(struct FsalUpdateAttributesRequest);
const size_t size_fsal_move_einode_request = sizeof(struct FsalMoveEinodeRequest);
const size_t size_fsal_populate_prefix_permission = sizeof(struct FsalPopulatePrefixPermission);
const size_t size_fsal_update_prefix_permission = sizeof(struct FsalUpdatePrefixPermission);
const size_t size_mds_update_prefix_permission = sizeof(struct MDSUpdatePrefixPermission);
const size_t size_mds_populate_prefix_permission = sizeof(struct MDSPopulatePrefixPermission);

int number_of_threads = 10;

void *zmq_context_context; 
int zmq_context_initialized=0;
char *serveraddress;

void registermds(char *address)
{
    serveraddress = malloc(256);
    sprintf(serveraddress, "%s\0", address);
}

struct socket_array_entry_t
{
	pthread_t thread_id;
        Fsal_Msg_Sequence_Number sequence;
	void* zmq_socket;
	void* zmq_context;
        void* zmq_request_mem;        
} socket_array_entry;

struct socket_array_entry_t* socket_array = NULL;
struct socket_array_entry_t* socket_entry = NULL;

// Create a socket for the communication with ganesha 
void setup_socket()
{
	    socket_entry = (struct socket_array_entry_t*) malloc(sizeof(struct socket_array_entry_t));
 
	    void *context = zmq_init(1);
            void *socket = zmq_socket(context, ZMQ_REQ);
            zmq_connect (socket, ZMQ_ROUTER_FRONTEND );

            socket_entry->zmq_socket = socket;
            socket_entry->zmq_context = context;
            socket_entry->zmq_request_mem = malloc(FSAL_MSG_LEN);
}


// Release the socket
void teardown_socket()
{
    if (socket_entry!=NULL)
    {
        zmq_close(socket_entry->zmq_socket);
        zmq_term(socket_entry->zmq_context);
            free(socket_entry->zmq_request_mem);
            free(socket_entry);
    }
}

void setup_socket_array()
{
    int number_of_sockets = number_of_threads;
    socket_array = (struct socket_array_entry_t*) malloc( number_of_sockets * sizeof( struct socket_array_entry_t ));
    int i;
    for(i = 0; i < number_of_sockets; i++)
    {
        socket_array[i].thread_id = 0;
        socket_array[i].zmq_socket = NULL;
        socket_array[i].zmq_request_mem = NULL;
        socket_array[i].sequence = 0;
    }
}

void* get_socket()
{
    int i;
    //printf("Get socket\n");
    int number_of_sockets = number_of_threads;
    
    if (!zmq_context_initialized)
    {
        
            setup_socket_array();
            zmq_context_context = zmq_init(1);
            zmq_context_initialized=1;
    }
    for(i = 0; i < number_of_sockets; i++)
    {
        //this thread is not registered yet
        if(socket_array[i].thread_id == 0)
        {
            socket_array[i].thread_id = pthread_self();            
            
            void *socket = zmq_socket(zmq_context_context, ZMQ_REQ);
            //printf("socket is on: %s \n", serveraddress);
            zmq_connect (socket, serveraddress );

            socket_array[i].zmq_socket = socket;
            socket_array[i].zmq_context = zmq_context_context;
            socket_array[i].zmq_request_mem = malloc(FSAL_MSG_LEN);
            socket_array[i].sequence = pthread_self();
            return &(socket_array[i]);
        }

        //we have seen this thread before know his socket
        if(socket_array[i].thread_id == pthread_self())
        {
            socket_array[i].sequence+=1;
            return &(socket_array[i]);
        }
    }
    printf("ERROR:no socket found\n");
}



// Return the socket
/*void* get_socket()
{
    if (socket_entry==NULL){setup_socket();}
                      return socket_entry;
}**/
///////////////////////////////////////////////////////////77

static const int maximal_retries = 3;

//#define MY_PRINT_TRUE

#define myprint(x,...)    metamyprint(x, __PRETTY_FUNCTION__,##__VA_ARGS__)
void metamyprint( const char *format, const char* method,...)
{
    #ifdef MY_PRINT_TRUE
    char *buffer = (char *) malloc(256);
    va_list args;
    va_start (args, format);
    vsprintf (buffer,format, args);
    va_end (args);
    printf("%d:%s:%s\n",pthread_self(),method,buffer);
    #endif //MY_PRINT_TRUE
}


inline void my_free_zmq_message(void *data, void *hint){}

/**
 * @brief Sends a zmq message to the metadata server and waits for the
 * result.
 *
 * @details  If (global) USE_TEST_MD_LAYER=1, the testmode is activated
 * and all messages will be processed by the fsal_zmq_test_wrapper.
 * Otherwise the messages are transfered via zmq to the metadata
 * server.
 *
 \* @param[in] p_data pointer to the memory that contains the zmq message.
 * @param[in] p_data_size size of the zmq message
 * @param[out] p_response pointer to the zmq_msg where the mds
 * response is written to.
 *
 * @return integer representing the error code. Zero = ok.
 * */
int zmq_send_and_receive(   void *p_data,
                            size_t data_size,
                            zmq_msg_t *p_response,
                            void *p_socket)
{
    myprint("start");
    int rc=fsal_global_ok;
    if (!USE_TEST_MD_LAYER)
    {
        zmq_msg_t msg;
#define HEAPFREE_MESSAGING        
#ifdef HEAPFREE_MESSAGING
        
        rc = zmq_msg_init_data(&msg, p_data, data_size ,my_free_zmq_message,NULL);
        myprint("start init data");
        if (!rc)
        {
            rc=zmq_msg_init(p_response);
            myprint("start init msg");
            if (!rc)
            {
                rc = zmq_send (p_socket, &msg, 0);
                myprint("sent");
                if (!rc)
                {
                    rc=zmq_recv(p_socket,p_response,0);
                    myprint("received");
                }
            }
        }
        zmq_msg_close (&msg);
#else
        rc = rebuild_and_fill_zmq_msg(p_data, data_size, &msg);
        if (!rc)
        {
            myprint("send request:socket=%p",p_socket);
            rc = zmq_send (p_socket, &msg, 0);
            if (!rc)
            {
                myprint("init response:socket=%p",p_socket);
                rc=zmq_msg_init(p_response);
                if (!rc)
                {
                    rc=zmq_recv(p_socket,p_response,0);
                    myprint("received:socket=%p, written to %p.",p_socket,p_response);
                }
            }
        }
        zmq_msg_close (&msg);        
#endif        
    }
    else
    {
        char *p_resp_data = (char *) malloc(FSAL_MSG_LEN);
        size_t resp_size;
        rc = fsal_zmq_send_and_receive_wrapper(p_data, data_size, p_resp_data, &resp_size);
        if (!rc)
        {
            rc = zmq_msg_init_size(p_response, resp_size);
            fsal_error_mapper(rc,zmq_msg_init_size_error);
            memcpy(zmq_msg_data(p_response),p_resp_data, resp_size);
        }
        free(p_resp_data);
    }
    myprint("RC:%d",rc);
    return rc;
}


/**
 * @brief Construct a FsalMoveEinodeRequest structure and use the
 * zmq_send_and_receive to get the result from the MDS.
 *
 * @param[in] p_partition_root_inode_number pointer to an InodeNumber
 * representing the root inode number of the partition the
 * file system object is placed in.
 * @param[in] p_inode_num pointer to an InodeNumber representing
 * the object that will be deleted.
 * @param[in] p_attrs structe containing attributes of an inode
 * @param[in] p_attribute_bitfield int pointer to the bitfield that
 * represents the changed attributes
 * */
int fsal_move_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_new_parent_inode_number,
    InodeNumber *p_old_parent_inode_number,
    FsObjectName *p_old_name,
    FsObjectName *p_new_name)
{
    int trycnt=0;
    struct socket_array_entry_t *p_socket_entry = (struct socket_array_entry_t*) get_socket();
    int rc = -1;
    zmq_msg_t response;
    struct FsalMoveEinodeRequest *p_fsal_req = (struct FsalMoveEinodeRequest*) p_socket_entry->zmq_request_mem;
    p_fsal_req->type = move_einode_request;
    p_fsal_req->new_parent_inode_number = *p_new_parent_inode_number;
    p_fsal_req->old_parent_inode_number = *p_old_parent_inode_number;
    p_fsal_req->seqnum = p_socket_entry->sequence;
    memcpy(&p_fsal_req->new_name,p_new_name,size_fsobject_name);
    memcpy(&p_fsal_req->old_name,p_old_name,size_fsobject_name);
    p_fsal_req->partition_root_inode_number = *p_partition_root_inode_number;    
    while (trycnt>maximal_retries)
    {
        rc=zmq_send_and_receive(p_fsal_req,
                                size_fsal_move_einode_request,
                                &response,
                                p_socket_entry->zmq_socket);
        if (!rc)
        {
            struct FsalMoveEinodeResponse *p_fsal_resp = (struct FsalMoveEinodeResponse*) zmq_msg_data(&response) ;
            if (p_fsal_resp->seqnum == p_socket_entry->sequence)
            {
                if (p_fsal_resp->type == move_einode_response)
                {
                        rc = p_fsal_resp->error;
                        break;
                }
            }
            myprint("Sequencenum sent:%u, sequencenum recv:%u",p_fsal_req->seqnum,p_fsal_resp->seqnum );
            myprint("Wrong response received:type=%d. Retry: sendig request",p_fsal_resp->type);
            trycnt++;
            myprint("incrementing sequence.");
            p_socket_entry->sequence+=1;
            p_fsal_req->seqnum = p_socket_entry->sequence;            
        }
        else
        {
            rc=-1;
            trycnt++;
        }
    }
    zmq_msg_close(&response);
    return rc;
}


/*param[in] p_partition_root_inode_number pointer to an InodeNumber
 * representing the root inode number of the partition the
 * file system object is placed in.
 * @param[in] p_parent_inode_num pointer to an InodeNumber object
 * representing the directory where the desired file system object is
 * placed in.
 * @param[in] p_name char pointer representing the name of the desired
 * file system object
 * @param[out] p_inode_num pointer to an InodeNumber structure that the
 * result is written to.
 *
 * @return integer representing the return code. Zero = ok.
 * */
int fsal_lookup_inode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_parent_inode_num,
    FsObjectName *p_name,
    InodeNumber *p_inode_num)
{
    int rc = -1;
    int trycnt=0;
    struct socket_array_entry_t *p_socket_entry = (struct socket_array_entry_t*) get_socket();
    size_t len_name = strlen(&(*p_name[0]));

    struct FsalLookupInodeNumberByNameResponse *p_inode_lookup_msg_resp = NULL;
    if (len_name >= MAX_NAME_LEN)
    {
        rc = fsal_lookup_inode_requested_name_exceeds_length_limits;
    }
    else
    {
        struct FsalLookupInodeNumberByNameRequest *p_inode_lookup_msg = (struct FsalLookupInodeNumberByNameRequest*) p_socket_entry->zmq_request_mem;
        p_inode_lookup_msg->type = lookup_inode_number_request;
        p_inode_lookup_msg->partition_root_inode_number = *p_partition_root_inode_number;
        p_inode_lookup_msg->parent_inode_number = *p_parent_inode_num;
        p_inode_lookup_msg->seqnum = p_socket_entry->sequence;
        //memset(&p_inode_lookup_msg->name,0,MAX_NAME_LEN);
        memcpy(&p_inode_lookup_msg->name, p_name, len_name);
        p_inode_lookup_msg->name[len_name] = '\0';
        myprint("Socket(%p).lookupinum of %s. To:%p",p_socket_entry->zmq_socket,*p_name,p_inode_num);
        zmq_msg_t response;
        while (trycnt<maximal_retries)
        {
            
            rc=zmq_send_and_receive(p_inode_lookup_msg,
                                    size_fsal_lookup_inode_number_by_name_request,
                                    &response,
                                    p_socket_entry->zmq_socket);
            myprint("rc:%u",rc);
            if (!rc)
            {
                p_inode_lookup_msg_resp = (struct FsalLookupInodeNumberByNameResponse*) zmq_msg_data(&response);
                if (p_inode_lookup_msg_resp->seqnum == p_socket_entry->sequence)
                {
                    if (p_inode_lookup_msg_resp->type == lookup_inode_number_response)
                    {
                        myprint("Src(%p).Socket(%p)Target(%p).lookupinum:res:%llu:resp:error=%d.MEM=%p",&response,p_socket_entry->zmq_socket,&p_inode_lookup_msg_resp,p_inode_lookup_msg_resp->inode_number,p_inode_lookup_msg_resp->error,p_socket_entry->zmq_request_mem);
                        myprint("responsetype=%d",p_inode_lookup_msg_resp->type);
                        *p_inode_num = p_inode_lookup_msg_resp->inode_number;
                        rc = p_inode_lookup_msg_resp->error;
                        break;
                    }
                }
                myprint("Sequencenum sent:%u, sequencenum recv:%u",p_inode_lookup_msg->seqnum,p_inode_lookup_msg_resp->seqnum);
                myprint("ERROR: wrong response received:type=%d.",p_inode_lookup_msg_resp->type);
                rc=-2;
                trycnt++;
                p_socket_entry->sequence+=1;
                p_inode_lookup_msg->seqnum = p_socket_entry->sequence;
            }
            else
            {
                myprint("Error: send_and_receive returned error:%d",rc);
                p_inode_num = NULL;
                trycnt++;
            }
        }
        zmq_msg_close(&response);
    }
    myprint("Socket(%p).lookupinum:res:%llu:resp:error=%d.MEM=%p",p_socket_entry,*p_inode_num,rc,p_socket_entry->zmq_request_mem);
    return rc;
}

/**
 * @brief Construct an FsalEInodeRequest structure an use the
 * zmq_send_and_receive to get the result from the MDS.
 *
 * @param[in] p_partition_root_inode_number pointer to an InodeNumber
 * representing the root inode number of the partition the
 * file system object is placed in.
 * @param[in] p_inode_num pointer to an InodeNumber representing the
 * requested file system object.
 * @param[out] p_einode pointer to an mds_inode_t structure. The resulting
 * inode will be written to this structure.
 *
 * @return integer representing the return code. Zero = ok.
 * */
int fsal_read_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_num,
    struct EInode *p_einode )
{
    int trycnt=0;
    int rc=-1;
    struct socket_array_entry_t *p_socket_entry = (struct socket_array_entry_t*) get_socket();
    struct FsalEInodeRequest *p_fsal_req = (struct FsalEInodeRequest*) p_socket_entry->zmq_request_mem;
    p_fsal_req->type = einode_request;
    p_fsal_req->seqnum = p_socket_entry->sequence;
    p_fsal_req->inode_number = *p_inode_num;
    p_fsal_req->partition_root_inode_number = *p_partition_root_inode_number;
    zmq_msg_t response;
    myprint("Socket(%p).read_einode(%llu):sending.",p_socket_entry->zmq_socket,*p_inode_num);
    while (trycnt<maximal_retries)
    {
        rc=zmq_send_and_receive(    p_fsal_req,
                                    size_fsal_einode_request,
                                    &response,
                                    p_socket_entry->zmq_socket);
        if (!rc)
        {
            struct FsalFileEInodeResponse *p_fsal_resp = (struct FsalFileEInodeResponse*) zmq_msg_data(&response);
            if (p_fsal_resp->seqnum == p_socket_entry->sequence)
            {
                if (p_fsal_resp->type == file_einode_response)
                {
                    memcpy(p_einode, &p_fsal_resp->einode, sizeof(struct EInode));
                    rc = p_fsal_resp->error;
                    myprint("responsetype=%u, error:%d",p_fsal_resp->type,rc);
                    myprint("Socket(%p).read_einode(%llu):name=%s:MEM:%p.",p_socket_entry->zmq_socket,*p_inode_num,&p_einode->name[0],p_socket_entry->zmq_request_mem);
                    break;
                }
            }
            myprint("Sequencenum sent:%u, sequencenum recv:%u",p_fsal_req->seqnum,p_fsal_resp->seqnum );
            myprint("ERROR: wrong response received:type=%d.",p_fsal_resp->type);
            rc=-2;
            trycnt++;
            p_socket_entry->sequence+=1;
            p_fsal_req->seqnum = p_socket_entry->sequence;
        }
        else
        {
            myprint("Error: send_and_receive returned error:%d",rc);
            trycnt++;
        }
    }
    zmq_msg_close(&response);
    myprint("Socket(%p).read_einode(%llu):name=%s:MEM:%p.",p_socket_entry->zmq_socket,*p_inode_num,&p_einode->name,p_socket_entry->zmq_request_mem);
    return rc;
}

/**
 * @brief Construct a FsalReaddirRequest structure and use the
 * zmq_send_and_receive to get the result from the MDS.
 *
 * @param[in] p_partition_root_inode_number pointer to an InodeNumber
 * representing the root inode number of the partition the
 * file system object is placed in.
 * @param[in] p_inode_num pointer to an InodeNumber representing the
 * requested file system object. *
 * @param[in] p_offset pointer to an ReaddirOffset type representing the
 * einode offset to retrieve file system objects from.
 * @param[out] p_rdir ReadDirReturn structure. The resulting
 * data will be written to this structure.
 *
 * @return integer representing the return code. Zero = ok.
 * */
int fsal_read_dir(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_num,
    ReaddirOffset *p_offset,
    struct ReadDirReturn *p_rdir)
{
    int trycnt=0;
    int rc = -1;
    struct socket_array_entry_t *p_socket_entry = (struct socket_array_entry_t*) get_socket();
    zmq_msg_t response;
    struct FsalReaddirRequest *p_fsal_req = (struct FsalReaddirRequest*) p_socket_entry->zmq_request_mem;
    p_fsal_req->type = read_dir_request;
    p_fsal_req->seqnum = p_socket_entry->sequence;
    p_fsal_req->partition_root_inode_number = (InodeNumber) *p_partition_root_inode_number;
    p_fsal_req->inode_number = (InodeNumber) *p_inode_num;
    p_fsal_req->offset = (ReaddirOffset) *p_offset;
    while (trycnt<maximal_retries)
    {
        rc=zmq_send_and_receive(    p_fsal_req,
                                    size_fsal_readdir_request,
                                    &response,
                                    p_socket_entry->zmq_socket);
        if (!rc)
        {
                struct FsalReaddirResponse *p_fsal_resp = (struct FsalReaddirResponse*) zmq_msg_data(&response);
                if (p_fsal_resp->seqnum == p_socket_entry->sequence)
                {
                    if (p_fsal_resp->type == read_dir_response)
                    {
                            memcpy(p_rdir,&p_fsal_resp->directory_content,sizeof(struct ReadDirReturn));
                            rc = p_fsal_resp->error;
                            break;
                    }
                }
                myprint("Sequencenum sent:%u, sequencenum recv:%u",p_fsal_req->seqnum,p_fsal_resp->seqnum );
                myprint("ERROR: wrong response received:type=%d",p_fsal_resp->type);
                rc=-2;
                trycnt++;
                p_socket_entry->sequence+=1;
                p_fsal_req->seqnum = p_socket_entry->sequence;     
        }
        else
        {
            trycnt++;
            myprint("Error: send_and_receive returned error:%d.",rc);
        }
    }
    zmq_msg_close(&response);
    return rc;
}

/**
 * @brief Construct a FsalCreateFileEInodeRequest structure and use the
 * zmq_send_and_receive to get the result from the MDS.
 *
 * @param[in] p_partition_root_inode_number pointer to an InodeNumber
 * representing the root inode number of the partition the
 * file system object is placed in.
 * @param[in] p_parent_inode_num pointer to an InodeNumber representing
 * the directory to place the object in.
 * @param[in] p_attrs attributes of the greated fs object
 * @param[out] p_return_inum inode number of the created object
 * */
int fsal_create_file_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_parent_inode_number,
    inode_create_attributes_t *p_attrs,
    InodeNumber *p_return_inum)
{
    int trycnt=0;
    int rc = -1;
    struct socket_array_entry_t *p_socket_entry = (struct socket_array_entry_t*) get_socket();
    zmq_msg_t response;
    struct FsalCreateEInodeRequest *p_fsal_req = (struct FsalCreateEInodeRequest*) p_socket_entry->zmq_request_mem;
    p_fsal_req->type = create_file_einode_request;
    p_fsal_req->seqnum = p_socket_entry->sequence;
    p_fsal_req->partition_root_inode_number = *p_partition_root_inode_number;
    p_fsal_req->parent_inode_number = *p_parent_inode_number;
    if (sizeof(p_fsal_req->attrs) >= sizeof(inode_create_attributes_t))
    {
        memcpy(&p_fsal_req->attrs, p_attrs, sizeof(inode_create_attributes_t));
        while (trycnt<maximal_retries)
        {
            rc=zmq_send_and_receive(    p_fsal_req, 
                                        size_fsal_create_einode_request,
                                        &response,
                                        p_socket_entry->zmq_socket);
            if (!rc)
            {
                struct FsalCreateInodeResponse *p_fsal_resp = (struct FsalCreateInodeResponse*) zmq_msg_data(&response);
                if (p_fsal_resp->seqnum == p_socket_entry->sequence)
                {

                    if (p_fsal_resp->type == create_inode_response)
                    {
                        *p_return_inum = p_fsal_resp->inode_number;
                        rc = p_fsal_resp->error;
                        myprint("create(%llu):MEM:%p.",*p_return_inum,p_socket_entry->zmq_request_mem);
                        break;
                    }
                }
                myprint("Sequencenum sent:%u, sequencenum recv:%u",p_fsal_req->seqnum,p_fsal_resp->seqnum );
                myprint("ERROR: wrong response received:type=%d.",p_fsal_resp->type);
                trycnt++;
                rc=-2;
                p_socket_entry->sequence+=1;
                p_fsal_req->seqnum = p_socket_entry->sequence;
            }
            else
            {
                myprint("ERROR: send_and_receive returned error:%d.",rc);
                trycnt++;
            }
        }
        zmq_msg_close(&response);
    }
    else
    {
        rc=fsal_create_file_einode_attrs_exceed_fsalattrs;
    }
    myprint("Socket(%p).create_file_einode:return=%d;inum=%llu.",p_socket_entry->zmq_socket,rc,*p_return_inum);
    return rc;
}

/**
 * @brief Construct a FsalDeleteInodeRequest structure and use the
 * zmq_send_and_receive to get the result from the MDS.
 *
 * @param[in] p_partition_root_inode_number pointer to an InodeNumber
 * representing the root inode number of the partition the
 * file system object is placed in.
 * @param[in] p_inode_num pointer to an InodeNumber representing
 * the object that will be deleted.
 * */
int fsal_delete_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_number)
{
    int trycnt = 0;
    int rc = -1;
    struct socket_array_entry_t *p_socket_entry = (struct socket_array_entry_t*) get_socket();
    zmq_msg_t response;
    struct FsalDeleteInodeRequest *p_fsal_req = (struct FsalDeleteInodeRequest*) p_socket_entry->zmq_request_mem;
    p_fsal_req->type = delete_inode_request;
    p_fsal_req->seqnum = p_socket_entry->sequence;
    p_fsal_req->partition_root_inode_number = *p_partition_root_inode_number;
    p_fsal_req->inode_number = *p_inode_number;
    while (trycnt<maximal_retries)
    {
            rc=zmq_send_and_receive(    p_fsal_req,
                                    size_fsal_delete_inode_request,
                                    &response,
                                    p_socket_entry->zmq_socket);
            if (!rc)
            {
                struct FsalDeleteInodeResponse *p_fsal_resp = (struct FsalDeleteInodeResponse*) zmq_msg_data(&response);
                if (p_fsal_resp->seqnum == p_socket_entry->sequence)
                {
                        if (p_fsal_resp->type == delete_inode_response)
                        {
                                rc = p_fsal_resp->error;
                                break;
                        }
                }
                myprint("Sequencenum sent:%u, sequencenum recv:%u",p_fsal_req->seqnum,p_fsal_resp->seqnum );
                myprint("ERROR: wrong response received:type=%d.",p_fsal_resp->type);
                rc=-2;
                trycnt++;
                p_socket_entry->sequence+=1;
                p_fsal_req->seqnum = p_socket_entry->sequence;
            }
            else
            {
                trycnt++;
                myprint("Error: send and receive retured error:%d.",rc);
            }
    }
    zmq_msg_close(&response);
    return rc;
}
    

/**
 * @brief Construct a FsalUpdateAttributesRequest structure and use the
 * zmq_send_and_receive to get the result from the MDS.
 *
 * @param[in] p_partition_root_inode_number pointer to an InodeNumber
 * representing the root inode number of the partition the
 * file system object is placed in.
 * @param[in] p_inode_num pointer to an InodeNumber representing
 * the object that will be deleted.
 * @param[in] p_attrs structe containing attributes of an inode
 * @param[in] p_attribute_bitfield int pointer to the bitfield that
 * represents the changed attributes
 * */
int fsal_update_attributes(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_num,
    inode_update_attributes_t *p_attrs,
    int *p_attribute_bitfield)
{
    struct socket_array_entry_t *p_socket_entry = (struct socket_array_entry_t*) get_socket();
    int rc = -1;
    int trycnt = 0;
    zmq_msg_t response;
    struct FsalUpdateAttributesResponse *p_fsal_resp = NULL;
    struct FsalUpdateAttributesRequest *p_fsal_req = (struct FsalUpdateAttributesRequest*)p_socket_entry->zmq_request_mem;
    p_fsal_req->type = update_attributes_request;
    p_fsal_req->seqnum = p_socket_entry->sequence;
    p_fsal_req->inode_number = *p_inode_num;
    p_fsal_req->attribute_bitfield=*p_attribute_bitfield;
    p_fsal_req->partition_root_inode_number = *p_partition_root_inode_number;
    if (sizeof(p_fsal_req->attrs)>=sizeof(inode_update_attributes_t))
    {
        memcpy(&p_fsal_req->attrs, p_attrs, sizeof(inode_update_attributes_t));
        while (trycnt<maximal_retries)
        {
            rc=zmq_send_and_receive(    p_fsal_req,
                                    size_fsal_update_attributes_request,
                                    &response,
                                    p_socket_entry->zmq_socket);
            if (!rc)
            {
                p_fsal_resp = (struct FsalUpdateAttributesResponse*) zmq_msg_data(&response) ;
                if (p_fsal_resp->seqnum == p_socket_entry->sequence)
                {
                    if (p_fsal_resp->type == update_attributes_response)
                    {
                        rc = p_fsal_resp->error;
                        myprint("responsetype=%u",p_fsal_resp->type);
                        myprint("Socket(%p)Target(%p).update_attrs(%llu):resp:error=%d;MEM:%p",p_socket_entry->zmq_socket,&p_fsal_resp,*p_inode_num,rc,p_socket_entry->zmq_request_mem);
                        break;
                    }
                }
                myprint("Sequencenum sent:%u, sequencenum recv:%u",p_fsal_req->seqnum,p_fsal_resp->seqnum );
                myprint("ERROR: wrong response received:type=%d",p_fsal_resp->type);
                rc=-2;
                trycnt++;
                p_socket_entry->sequence+=1;
                p_fsal_req->seqnum = p_socket_entry->sequence;
            }
            else
            {
                myprint("update_attrs(%llu):ERROR_occured_while_sending:error=%d.",*p_inode_num,rc);
                trycnt++;
            }
        }
    }
    else
    {
        rc=fsal_update_attributes_attrs_exceed_fsalattrs;
    }
    zmq_msg_close(&response);
    myprint("Socket(%p).update_attrs(%llu):resp:error=%d;MEM:%p",p_socket_entry->zmq_socket,*p_inode_num,rc,p_socket_entry->zmq_request_mem);    return rc;
}
