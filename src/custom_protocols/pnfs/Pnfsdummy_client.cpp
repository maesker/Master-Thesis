/* 
 * File:   Pnfsdummy_client.cpp
 * Author: markus
 * 
 * Created on 6. Januar 2012, 20:32
 */

#include "custom_protocols/pnfs/Pnfsdummy_client.h"


/** 
 * @brief Free zmq message data
 * @param[in] data pointer to the zmq message data to free
 * @param[in] hint dont know, in my case always  null.
 * 
 * This function is used by the zmq library to free the data after sending a 
 * message. 
 * */
static inline void dummyfree_zmq_message(void *data, void *hint){}


Pnfsdummy_client::Pnfsdummy_client(Logger *p_log) 
{    
    sm = new SocketManager<struct mds_socket>();
    log = p_log;
    root = 1;
    uid = 1000;
    gid = 1000;
    
    setup_socket_array();
}

Pnfsdummy_client::Pnfsdummy_client(const Pnfsdummy_client& orig) {
}

Pnfsdummy_client::~Pnfsdummy_client() {
    log->debug_log("start");
    std::map<serverid_t,struct mds_socket*>::iterator it1 = sm->socketmap.begin();
    for (it1; it1!=sm->socketmap.end(); it1++)
    {
        log->debug_log("it1 start");
        pthread_mutex_destroy(&it1->second->mutex);
        zmq_close(it1->second->zmq_socket);
        zmq_term(it1->second->zmq_context);
        free(it1->second->zmq_memspace);
        free(it1->second);
        log->debug_log("it1 end");
    }
    teardown_socket();
    delete sm;
}

int Pnfsdummy_client::addMDS(serverid_t id, std::string ip, uint16_t port)
{
    log->debug_log("id:%u,ip:%s, port:%u",id, ip.c_str(), port);
    int rc = sm->addServer(id,ip,port);
    if (!rc)
    {
        log->debug_log("connecting...");
        rc = connectMDS(id);
    }
    log->debug_log("rc:%d",rc);
    return rc;
}


int Pnfsdummy_client::connectMDS(serverid_t id)
{
    struct mds_socket *mdssocket  = (struct mds_socket*) malloc(sizeof(struct mds_socket));
    void *context = zmq_init(1);
    void *socket = zmq_socket(context, ZMQ_REQ);

    std::string fr = sm->getTcpAddress(id);
    registermds((char *) fr.c_str());
    //printf("Connect:%s\n",fr.c_str());
    zmq_connect (socket, fr.c_str());

    mdssocket->zmq_socket = socket;
    mdssocket->zmq_context = context;
    mdssocket->zmq_memspace = malloc(pnfs_zmq_max_msg_len);
    mdssocket->sequencenumber = pthread_self();
    mdssocket->mutex = PTHREAD_MUTEX_INITIALIZER;
    //mdssocket->protocol = new PnfsProtocol();
    sm->addSocket(id,mdssocket);
    log->debug_log("Connected to %d:%p.\n",id,mdssocket);
    return 0;
}

/**
 * */
int Pnfsdummy_client::handle_pnfs_send_helloworld(serverid_t id)
{
    int rc = -1;
    zmq_msg_t response;
    log->debug_log("server id:%u",id);
    struct mds_socket *p_sock = sm->getSocket(id);
    struct PNFS_HelloWorld_req_t *p_req = (struct PNFS_HelloWorld_req_t*) p_sock->zmq_memspace;
    p_req->head.msg_type = MT_helloworld_req;

    std::string c = std::string("Hello");
    snprintf(&p_req->data[0], 6, c.c_str());   
    //printf("pnfs:sending..hello.\n");
    rc=this->send_and_recv(     p_req,
                                sizeof(struct PNFS_HelloWorld_req_t),
                                &response,
                                p_sock);
    if (!rc)
    {
        struct PNFS_HelloWorld_resp_t *p_res = (struct PNFS_HelloWorld_resp_t*) zmq_msg_data(&response) ;
      //  printf("%s...\n", &p_res->data );
    }
    zmq_msg_close(&response);  
    log->debug_log("rc:%d",rc);
    return rc;
}


/**
 * */
int Pnfsdummy_client::handle_pnfs_send_createsession(serverid_t id, ClientSessionId *csid)
{
    int rc = -1;
    zmq_msg_t response;
    log->debug_log("server id:%u, gid:%u, uid:%u",id,gid,uid);
    struct mds_socket *p_sock = sm->getSocket(id);
    struct PNFS_CreateSession_req_t *p_req = (struct PNFS_CreateSession_req_t*) p_sock->zmq_memspace;
    p_req->head.msg_type = PNFS_CreateSession_req;
    p_req->gid = gid;
    p_req->uid = uid;
    rc=this->send_and_recv(     p_req,
                                sizeof(struct PNFS_CreateSession_req_t),
                                &response,
                                p_sock);
    if (!rc)
    {
        struct PNFS_CreateSession_resp_t *p_res = (struct PNFS_CreateSession_resp_t*) zmq_msg_data(&response) ;
        //printf("Sessionid:%llu-%u-%u.\n", p_res->csid,p_req->uid, p_req->gid);
        *csid = p_res->csid;
        log->debug_log("csid:%u",*csid);
        rc = 0;
    }
    zmq_msg_close(&response);
    log->debug_log("rc:%d",rc);
    return rc;
}

/**
 * */
int Pnfsdummy_client::handle_pnfs_send_byterangelock(serverid_t *id, 
        ClientSessionId *csid, InodeNumber *inum, uint64_t *start, uint64_t *end, time_t *expires)
{
    int rc = -1;
    zmq_msg_t response;
    log->debug_log("serverid:%u, csid:%u, inum:%llu, range:%llu-%llu",*id,*csid,*inum,*start,*end);
    struct mds_socket *p_sock = sm->getSocket(*id);
    log->debug_log("got socket");
    struct PNFS_ByterangeLock_req_t *p_req = (struct PNFS_ByterangeLock_req_t*) p_sock->zmq_memspace;
    p_req->head.msg_type = PNFS_ByterangeLock_req;
    p_req->end = *end;
    p_req->start = *start;
    p_req->csid = *csid;
    p_req->inum = *inum;
    rc=this->send_and_recv(     p_req,
                                sizeof(struct PNFS_ByterangeLock_req_t),
                                &response,
                                p_sock);
    if (!rc)
    {
        struct PNFS_ByterangeLock_resp_t *p_res = (struct PNFS_ByterangeLock_resp_t*) zmq_msg_data(&response) ;
        log->debug_log("acquired brl:%u.\n", p_res->expires);
        *expires = p_res->expires;
        if (p_res->expires==0)
        {
            rc=-1;
        }
        log->debug_log("acquired.");
    }
    zmq_msg_close(&response);
    log->debug_log("rc:%d",rc);
    return rc;
}

int Pnfsdummy_client::handle_pnfs_send_releaselock(serverid_t *id, ClientSessionId *csid, InodeNumber *inum, uint64_t *start, uint64_t *end)
{
    int rc = -1;
    zmq_msg_t response;
    log->debug_log("serverid:%u, csid:%u, inum:%u, ranget:%u-%u",*id,*csid,*inum,*start,*end);
    struct mds_socket *p_sock = sm->getSocket(*id);
    log->debug_log("got socket");
    struct PNFS_ReleaseLock_req_t *p_req = (struct PNFS_ReleaseLock_req_t*) p_sock->zmq_memspace;
    p_req->head.msg_type = PNFS_ReleaseLock_req;
    p_req->end = *end;
    p_req->start = *start;
    p_req->csid = *csid;
    p_req->inum = *inum;
    rc=this->send_and_recv(     p_req,
                                sizeof(struct PNFS_ByterangeLock_req_t),
                                &response,
                                p_sock);
    if (!rc)
    {
        struct PNFS_ReleaseLock_resp_t *p_res = (struct PNFS_ReleaseLock_resp_t*) zmq_msg_data(&response) ;
        //printf("acquired brl:%d.\n", p_res->expires);
        log->debug_log("released.");
    }
    zmq_msg_close(&response);
    log->debug_log("rc:%d",rc);
    return rc;
}

int Pnfsdummy_client::handle_pnfs_send_dataserverlayout(serverid_t id, uint32_t *dscount)
{
    int rc;
    log->debug_log("server id: %u",id);
    struct mds_socket *p_sock = sm->getSocket(id);
    struct PNFS_DataserverLayout_req_t *p_req = (struct PNFS_DataserverLayout_req_t*) p_sock->zmq_memspace;
    p_req->head.msg_type = PNFS_DataserverLayout_req;
    zmq_msg_t response;
    rc=this->send_and_recv(     p_req,
                                sizeof(struct PNFS_DataserverLayout_req_t),
                                &response,
                                p_sock);
    if (!rc)
    {
        struct PNFS_DataserverLayout_resp_t *p_res = (struct PNFS_DataserverLayout_resp_t*) zmq_msg_data(&response) ;
     //   printf("%u dataserver registered.\n", p_res->count);
        *dscount = p_res->count;
        log->debug_log("dscount:%d",*dscount);
        rc = 0;
    }
    zmq_msg_close(&response);
    log->debug_log("rc:%d",rc);
    return rc;
}



int Pnfsdummy_client::handle_pnfs_send_dataserver_address(serverid_t *mds, serverid_t *dsid, ipaddress_t *address)
{
    int rc;
    log->debug_log("serverid %u, dsid:%u",*mds, *dsid);
    struct mds_socket *p_sock = sm->getSocket(*mds);
    struct PNFS_DataserverAddress_req_t *p_req = (struct PNFS_DataserverAddress_req_t*) p_sock->zmq_memspace;
    p_req->head.msg_type = PNFS_DataserverAddress_req;
    p_req->dsid = *dsid;
    zmq_msg_t response;
    rc=this->send_and_recv(     p_req,
                                sizeof(struct PNFS_DataserverAddress_req_t),
                                &response,
                                p_sock);
    if (!rc)
    {
        struct PNFS_DataserverAddress_resp_t *p_res = (struct PNFS_DataserverAddress_resp_t*) zmq_msg_data(&response) ;
        log->debug_log("dataserver %u registered at %s.\n", *dsid, &p_res->address[0]);
        strncpy(address[0], &p_res->address[0], sizeof(ipaddress_t));
        rc = 0;
    }
    zmq_msg_close(&response);        
    log->debug_log("rc:%u",rc);
    return rc;
}


int Pnfsdummy_client::send_and_recv(    void *p_data,
                                        size_t data_size,
                                        zmq_msg_t *p_response,
                                        struct mds_socket *socket)
{
    int rc=-1;
    zmq_msg_t msg;
    struct custom_protocol_reqhead_t *p_head_req = (struct custom_protocol_reqhead_t *) p_data;
    p_head_req->protocol_id = pnfs_id;
    p_head_req->sequence_number = ++socket->sequencenumber;
    log->debug_log("seqnum:%u",p_head_req->sequence_number);
    rc = zmq_msg_init_data(&msg, p_data, data_size ,dummyfree_zmq_message,NULL);
    try
    {
        if (!rc)
        {
            rc=zmq_msg_init(p_response);
            if (!rc)
            {
                log->debug_log("initialized response.");
                rc = zmq_send (socket->zmq_socket, &msg, 0);
                if (!rc)
                {
                    log->debug_log("reading data.");
                    rc=zmq_recv(socket->zmq_socket,p_response,0);
                    log->debug_log("read");
                    struct custom_protocol_resphead_t *p_head_resp = (struct custom_protocol_resphead_t *) zmq_msg_data(p_response);
                    if (p_head_resp->protocol_id    != p_head_req->protocol_id ||
                        p_head_resp->msg_type       != p_head_req->msg_type+1 ||
                        p_head_resp->sequence_number!= p_head_req->sequence_number ||
                        p_head_resp->error.major_id != 0 ||
                        p_head_resp->error.minor_id != 0) 
                    {
                        rc=1;
                        log->debug_log("SnR:%d:error\n",rc);
                        log->error_log("protocol:%d:%d",p_head_resp->protocol_id,p_head_req->protocol_id);
                        log->error_log("msgtype :%u:%d+1",p_head_resp->msg_type,p_head_req->msg_type);
                        log->error_log("seqnum:%u:%u",p_head_resp->sequence_number,p_head_req->sequence_number);
                        log->error_log("errormaj:%u",p_head_resp->error.major_id);
                        log->error_log("errormin:%u",p_head_resp->error.minor_id);                    
                    }
                    else
                    {
                        log->debug_log("recv ok.");
                    }                
                }
                else
                {
                    log->debug_log("failed to send message");
                }
            }
            else
            {
                log->warning_log("failed to initialize message");
            }
        }
    }
    catch (...)
    {
        log->debug_log("Error occured while sending");
    }
    pthread_mutex_unlock(&socket->mutex);
    log->debug_log("rc:%d",rc);
    zmq_msg_close (&msg);
    return rc;
}
    




int Pnfsdummy_client::meta_get_file_inode(InodeNumber *partition_root, InodeNumber *p_inum, struct EInode *p_einode)
{
    log->debug_log("partition:%llu, inum:%llu",*partition_root,*p_inum);
    int rc = fsal_read_einode(partition_root, p_inum,p_einode);
    if (!rc)
    {
//        printf("create:rc=%d\n",rc);
        log->debug_log("Got filename:%s.\n",&p_einode->name);
//        printf("Got uid:%u\n",p_einode->inode.uid);
    }
    else
    {
        log->error_log("Error occured.");
    }
    return rc;
}


int Pnfsdummy_client::meta_create_file(InodeNumber *parent, FsObjectName *name, InodeNumber *p_res)
{
    return meta_create_file(&root,parent,name,p_res);
}

int Pnfsdummy_client::meta_create_file(InodeNumber *partition_root, InodeNumber *parent, FsObjectName *name, InodeNumber *p_res)
{
    log->debug_log("partition:%llu,parent:%llu,name:%s",partition_root,*parent,*name);
    int rc=-1;
    rc = fsal_lookup_inode(partition_root,parent, name, p_res);
    if (rc)
    {
        inode_create_attributes_t *p_attrs = gen_create_inode_attributes(name);
        rc = fsal_create_file_einode(partition_root, parent, p_attrs, p_res);
        free(p_attrs);
        log->debug_log("create:inum:%llu,rc=%d\n",*p_res,rc);
    }
    else
    {
        log->error_log("File exists.\n");
    }
    log->debug_log("rc:%d",rc);
    return rc;
}

inode_create_attributes_t* Pnfsdummy_client::gen_create_inode_attributes(FsObjectName *name)
{
    log->debug_log("name:%s",*name);
    inode_create_attributes_t *p_attrs = (inode_create_attributes_t*) malloc(sizeof(inode_create_attributes_t));
    p_attrs->mode = 0777;
    p_attrs->size = 0;
    strncpy(&p_attrs->name[0], name[0], MAX_NAME_LEN);
    p_attrs->name[strlen(p_attrs->name)] = '\0';
    p_attrs->uid = uid;
    p_attrs->gid = gid;
    log->debug_log("done.");
    return p_attrs;
}