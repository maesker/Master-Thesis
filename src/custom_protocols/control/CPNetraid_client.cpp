/* 
 * File:   CPNetraid_client.cpp
 * Author: markus
 * 
 * Created on 10. Januar 2012, 19:13
 */
#include <math.h>

#include "custom_protocols/control/CPNetraid_client.h"


void* multicast(void *mcast);

inline void free_zmq_msgmem(void *data, void *hint){}

CPNetraid_client::CPNetraid_client(Logger *p_log) {
    struct cp_socket_t sock;
    log = p_log;
    sm = new SocketManager<struct cp_socket_t>();
    p_ssh = new sshwrapper();
}

CPNetraid_client::CPNetraid_client(const CPNetraid_client& orig) {
}

CPNetraid_client::~CPNetraid_client() {
}


int CPNetraid_client::setupDS(std::string user,std::string bin, std::string pw)
{
/*    char str [256];
    memset(&str[0],0,256);
    printf ("Password:");
    
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~ECHO;
    (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
    gets (str);
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag |= ECHO;
    (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
    **/
    std::string mkdircmd = std::string("mkdir /etc/r2d2");
    std::string ss;
    //std::string pw = std::string(str);
    struct serverstruct_t srv;
    for (int i=0;i<sm->getServerNumber(); i++)
    {
        usleep(2000);
        std::ostringstream cmd("");
        cmd << bin << " " << i ;

        sm->getServer(i, &srv);
        std::string s = std::string(srv.ip);
        //printf("User:%s.cmd:%s.\n",user.c_str(),cmd.str().c_str());
        std::string conf = std::string(DS_CONFIG_FILE);
        
        p_ssh->send_cmd(s,user,pw,mkdircmd.c_str(),ss);
        p_ssh->scp_write(s,user,pw, conf, conf);
        p_ssh->send_cmd(s,user,pw,cmd.str().c_str(),ss);
        log->debug_log("done:%s",ss.c_str());
        ss.clear();    
    }
}

int CPNetraid_client::addDS(serverid_t id, std::string ip, uint16_t port)
{
    log->debug_log("id:%u, ip:%s, port:%u",id,ip.c_str(),port);
    sm->addServer(id,ip,port);
    return 0;
}


int CPNetraid_client::connectDS(serverid_t id)
{
    log->debug_log("id:%u",id);
    struct cp_socket_t *cpsocket  = (struct cp_socket_t*) malloc(sizeof(struct cp_socket_t));
    void *context = zmq_init(1);
    void *socket = zmq_socket(context, ZMQ_REQ);

    std::string fr = sm->getTcpAddress(id);
    //printf("Connect:%s\n",fr.c_str());
    zmq_connect (socket, fr.c_str());

    cpsocket->zmq_socket = socket;
    cpsocket->zmq_context = context;
    cpsocket->zmq_memspace = malloc(cpn_max_zmq_msg_len);
    cpsocket->sequencenumber = (uint32_t) (time(NULL)*id)%1000000;
    sm->addSocket(id,cpsocket);
    log->debug_log("Connected to %d:%p.\n",id,cpsocket);
    return 0;
}


int CPNetraid_client::send_and_recv(    void *p_data,
                                        size_t data_size,
                                        zmq_msg_t *p_response,
                                        struct cp_socket_t *p_sock)
{
    int rc=-1;
    log->debug_log("size:%u",data_size);
    zmq_msg_t msg;
    struct custom_protocol_reqhead_t *p_head_req = (struct custom_protocol_reqhead_t *) p_data;
    p_head_req->protocol_id = cpn_id;
    p_head_req->sequence_number = ++p_sock->sequencenumber;
    rc = zmq_msg_init_data(&msg, p_data, data_size ,free_zmq_msgmem,NULL);
    if (!rc)
    {
        rc=zmq_msg_init(p_response);
        if (!rc)
        {
            rc = zmq_send (p_sock->zmq_socket, &msg, 0);
            //printf("")
            if (!rc)
            {
                rc=zmq_recv(p_sock->zmq_socket,p_response,0);
                struct custom_protocol_resphead_t *p_head_resp = (struct custom_protocol_resphead_t *) zmq_msg_data(p_response);
                if (p_head_resp->protocol_id    != p_head_req->protocol_id ||
                    p_head_resp->msg_type       != p_head_req->msg_type+1 ||
                    p_head_resp->sequence_number!= p_head_req->sequence_number ||
                    p_head_resp->error.major_id != 0 ||
                    p_head_resp->error.minor_id != 0) 
                {
                    rc=1;
                    log->error_log("response error");
                }                
            }
        }
    }
    pthread_mutex_unlock(&p_sock->mutex);
    zmq_msg_close (&msg);
    log->debug_log("rc=%d",rc);
    return rc;
}



/**
 * */
int CPNetraid_client::handle_helloworld(socketid_t id)
{
    int rc = -1;
    struct cp_socket_t *s = sm->getSocket(id);
    if (s!=NULL)
    {
        zmq_msg_t response;
        struct CPN_HelloWorld_req_t *p_req = (struct CPN_HelloWorld_req_t*) s->zmq_memspace;
        p_req->head.msg_type = MT_helloworld_req;

        std::string c = std::string("Hello");
        snprintf(&p_req->data[0], 6, c.c_str());   
        rc=this->send_and_recv(     p_req,
                                    sizeof(struct CPN_HelloWorld_req_t),
                                    &response,
                                    s);
        if (!rc)
        {
            struct CPN_HelloWorld_resp_t *p_res = (struct CPN_HelloWorld_resp_t*) zmq_msg_data(&response) ;
            //printf("%s...\n", &p_res->data );
        }
        zmq_msg_close(&response);
    }

    return rc;
}


/**
 * */
int CPNetraid_client::multicast_distribute_Lock(std::list<socketid_t> *p_sockets, lock_exchange_struct *lock)
{
    log->debug_log("start");
    int rc = -1;
    int error = 0;
    struct CPN_LockDistribution_req_t req;
    std::map<pthread_t,struct cp_multicast_object*> threads;
    req.lock = *lock;
    req.head.msg_type = CP_DistricuteLock_req;
    req.head.sequence_number = (uint32_t)pthread_self();
    req.head.creation_time = time(NULL);
    for (std::list<socketid_t>::iterator it = p_sockets->begin(); it != p_sockets->end(); it++)
    {
        //printf("send to socket %d.\n",*it);
        pthread_t t;
        struct cp_multicast_object *mcast = new struct cp_multicast_object;
        mcast->msgdata = &req;
        mcast->msgsize = sizeof(req);
        mcast->p_sock = sm->getSocket(*it);
        mcast->retval = (int*)malloc(sizeof(int));
        pthread_create( &t, NULL, multicast,  mcast);
        threads.insert(std::pair<pthread_t,struct cp_multicast_object*>(t,mcast));
    }
    usleep(1000);
    for (std::map<pthread_t,struct cp_multicast_object*>::iterator it2 = threads.begin(); it2 != threads.end(); it2++)
    {
        log->debug_log("Threadid %u join. \n",it2->first);
        pthread_join(it2->first,NULL);        
        if (*it2->second->retval)
        {
            log->error_log("RC:%d.\n",*it2->second->retval);
            error++;
        }
        pthread_mutex_unlock(&it2->second->p_sock->mutex);
        free(it2->second->retval);
        free(it2->second);                
    }
    log->debug_log("Multicasted:%d errors.\n", error);
    return rc;
}


void* multicast(void *mcast)
{
    int rc;
    struct cp_multicast_object *mc = (struct cp_multicast_object *) mcast;
    
    zmq_msg_t response,msg;
    //printf("sock address:%p\n",mc->p_sock);
    
    struct custom_protocol_reqhead_t *p_head_req = (struct custom_protocol_reqhead_t *) mc->msgdata;
    p_head_req->protocol_id = cpn_id;
   
    rc = zmq_msg_init_data(&msg, mc->msgdata, mc->msgsize ,free_zmq_msgmem,NULL);
    if (!rc)
    {
        rc=zmq_msg_init(&response);
        if (!rc)
        {
            rc = zmq_send (mc->p_sock->zmq_socket, &msg, ZMQ_NOBLOCK);
            if (!rc)
            {
                zmq_pollitem_t items[1];
                items[0].socket = mc->p_sock->zmq_socket;
                items[0].fd = 0;
                items[0].events = ZMQ_POLLIN;    
                rc = zmq_poll(items, 1, 5000000);
                if(items[0].revents & ZMQ_POLLIN)
                {
                    rc=zmq_recv(mc->p_sock->zmq_socket,&response,0);
                    struct custom_protocol_resphead_t *p_head_resp = (struct custom_protocol_resphead_t *) zmq_msg_data(&response);
                    if (p_head_resp->protocol_id    != p_head_req->protocol_id ||
                        p_head_resp->sequence_number!= p_head_req->sequence_number ||
                        p_head_resp->error.major_id != 0 ||
                        p_head_resp->error.minor_id != 0 ||
                        rc) 
                    {
                        printf("Multicast msg not successful.\n");
                        //print_custom_protocol_reqhead(p_head_req);
                        //print_custom_protocol_resphead(p_head_resp);
                        rc=-1;
                    }
                }
                else
                {
                    printf("recv timed out.\n");
                    rc=-1;
                }
            }
            else
            {
                printf("send:rc=%d\n",rc);
                rc=-2;
            }
        }
        else
        {
            rc=-3;
        }
    }
    else
    {
        rc=-4;
    }
    zmq_msg_close (&msg);
    *mc->retval = rc;
}