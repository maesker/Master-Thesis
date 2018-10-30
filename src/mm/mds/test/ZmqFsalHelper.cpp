#include "mm/mds/test/ZmqFsalHelper.h"

zmq::context_t context(1);
ZmqFsalHelper::ZmqFsalHelper()
{
    
}

int32_t ZmqFsalHelper::send_msg_and_recv(void *p_data ,size_t size, void *p_result, size_t result_size)
{
    int32_t rc=0;

    zmq_msg_t request;
    zmq_msg_t response;
    
    zmq::socket_t responder(context, ZMQ_REQ);
    responder.connect(ZMQ_ROUTER_FRONTEND);
    rc = zmq_msg_init_size(&request, size);
    if (!rc)
    {
        // write data to zmq message
        memcpy(zmq_msg_data(&request),p_data,size);
        rc = zmq_send( responder , &request, 0);
        if (!rc)
        {
            //receiving response
            zmq_msg_init_size(&response, FSAL_MSG_LEN);
            rc = zmq_recv( responder , &response, 0);
            zmq_response_to_fsal_structure(p_result,result_size,&response);
        }
        else
        {
            rc=-2;
        }
    }
    responder.close();
    zmq_msg_close(&request);
    zmq_msg_close(&response);
    return rc;
}

InodeNumber ZmqFsalHelper::create_file(InodeNumber partition_root_inode_number, InodeNumber parent_inode_number)
{
    int32_t rc;
    InodeNumber ret = 0;
    // create and initialize the fsal request structure
    struct FsalCreateInodeResponse fsal_resp;
    FsalCreateEInodeRequest *ptr = get_mock_FsalCreateFileInodeRequest();
    ptr->partition_root_inode_number = partition_root_inode_number;
    ptr->parent_inode_number = parent_inode_number;

    // sned zmq request to message router and receive the result.
    rc = this->send_msg_and_recv(ptr ,sizeof(FsalCreateEInodeRequest), &fsal_resp,sizeof(fsal_resp));

    if ((rc==0) && (0 == fsal_resp.error) )
    {
        ret = fsal_resp.inode_number;
    }
    delete ptr;
    return ret;
}
