
#ifndef ZMQ_FSAL_HELPER_H_
#define ZMQ_FSAL_HELPER_H_


#include "global_types.h"
#include "message_types.h"
#include "mm/mds/MessageRouter.h"
#include "mm/mds/mds_journal_interface_lib.h"
#include "mm/mds/MockupObjects.h"

#include <zmq.hpp>
#include <zmq.h>


class ZmqFsalHelper
{
public:
    ZmqFsalHelper();
    InodeNumber create_file(InodeNumber partition_root_inode_number, InodeNumber parent_inode_number);
    int32_t send_msg_and_recv(void *p_data ,size_t size, void *p_result, size_t result_size);
};

#endif
