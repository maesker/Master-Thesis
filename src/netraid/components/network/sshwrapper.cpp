#include "components/network/sshwrapper.h"



static int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;
 
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
 
    FD_ZERO(&fd);
 
    FD_SET(socket_fd, &fd);
 
    /* now make sure we wait in the correct direction */ 
    dir = libssh2_session_block_directions(session);

 
    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;
 
    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;
 
    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);
 
    return rc;
}
 
#define BUFSIZE 32000
 

static int _send_cmd(std::string& host, std::string& user, std::string& pw,const  char *cmd, std::string& ss)
{
    unsigned long hostaddr;
    int sock;
    struct sockaddr_in sin;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    int rc;
    int exitcode;
    char *exitsignal=(char *)"none";
    int bytecount = 0;
    LIBSSH2_KNOWNHOSTS *nh;
 
    rc = libssh2_init (0);

    if (rc != 0) {
        fprintf (stderr, "libssh2 initialization failed (%d)\n", rc);
        return 1;
    }
 
    hostaddr = inet_addr(host.c_str());
 
    /* Ultra basic "connect to port 22 on localhost"
     * Your code is responsible for creating the socket establishing the
     * connection
     */ 
    sock = socket(AF_INET, SOCK_STREAM, 0);
 
    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = hostaddr;
    if (connect(sock, (struct sockaddr*)(&sin),
                sizeof(struct sockaddr_in)) != 0) {
        fprintf(stderr, "failed to connect!\n");
        return -1;
    }
 
    /* Create a session instance */ 
    session = libssh2_session_init();

    if (!session)
        return -1;
 
    /* tell libssh2 we want it all done non-blocking */ 
    libssh2_session_set_blocking(session, 0);

 
    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */ 
    while ((rc = libssh2_session_handshake(session, sock)) ==

           LIBSSH2_ERROR_EAGAIN);
    if (rc) {
        fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
        return -1;
    }
 
    nh = libssh2_knownhost_init(session);
    if(!nh) {
        /* eeek, do cleanup here */ 
        return 2;
    }
 
    /* read all hosts from here */ 
    libssh2_knownhost_readfile(nh, "known_hosts", LIBSSH2_KNOWNHOST_FILE_OPENSSH);
 
    /* store all known hosts to here */ 
    libssh2_knownhost_writefile(nh, "dumpfile", LIBSSH2_KNOWNHOST_FILE_OPENSSH); 

    libssh2_knownhost_free(nh);
 
    if ( pw.size() != 0 ) {
        /* We could authenticate via password */ 
        while ((rc = libssh2_userauth_password(session, user.c_str(), pw.c_str())) == LIBSSH2_ERROR_EAGAIN);
        if (rc) {
            fprintf(stderr, "Authentication by password failed.\n");
            goto shutdown;
        }
    }
    else {
        /* Or by public key */ 
        while ((rc = libssh2_userauth_publickey_fromfile(session, user.c_str(),
                                                         "/home/user/"
                                                         ".ssh/id_rsa.pub",
                                                         "/home/user/"
                                                         ".ssh/id_rsa",
                                                         pw.c_str())) ==
               LIBSSH2_ERROR_EAGAIN);
        if (rc) {
            fprintf(stderr, "\tAuthentication by public key failed\n");
            goto shutdown;
        }
    } 
 
    /* Exec non-blocking on the remove host */ 
    while( (channel = libssh2_channel_open_session(session)) == NULL &&
           libssh2_session_last_error(session,NULL,NULL,0) ==
           LIBSSH2_ERROR_EAGAIN )
    {
        waitsocket(sock, session);
    }
    if( channel == NULL )
    {
        fprintf(stderr,"Error\n");
        exit( 1 );
    }
    
    while( (rc = libssh2_channel_exec(channel, cmd)) == LIBSSH2_ERROR_EAGAIN )
    {
        waitsocket(sock, session);
    }
    if( rc != 0 )
    {
        fprintf(stderr,"Error\n");
        exit( 1 );
    }
    for( ;; )
    {
        /* loop until we block */ 
        int rc;
        do
        {
            char buffer[0x4000];
            rc = libssh2_channel_read( channel, buffer, sizeof(buffer) );

            if( rc > 0 )
            {
                int i;
                bytecount += rc;
                fprintf(stderr, "We read:\n");
                for( i=0; i < rc; ++i )
                    fputc( buffer[i], stderr);
                fprintf(stderr, "\n");
            }
            else {
                if( rc != LIBSSH2_ERROR_EAGAIN )
                    /* no need to output this for the EAGAIN case */ 
                    fprintf(stderr, "libssh2_channel_read returned %d\n", rc);
            }
            ss=std::string(&buffer[0]);
        }
        while( rc > 0 );
 
        /* this is due to blocking that would occur otherwise so we loop on
           this condition */ 
        //if( rc == LIBSSH2_ERROR_EAGAIN )
        //{
        //    waitsocket(sock, session);
        //}
        //else
            break;
    }
    exitcode = 127;
    while( (rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN )

        waitsocket(sock, session);
 
    if( rc == 0 )
    {
        exitcode = libssh2_channel_get_exit_status( channel );
        libssh2_channel_get_exit_signal(channel, &exitsignal, NULL, NULL, NULL, NULL, NULL);
    }
 
    if (exitsignal)
        printf("\nGot signal: %s\n", exitsignal);
    else
        printf("\nEXIT: %d bytecount: %d\n", exitcode, bytecount);
 
    libssh2_channel_free(channel);

    channel = NULL;
 
shutdown: 
    libssh2_session_disconnect(session,"Normal Shutdown, Thank you for playing");
    libssh2_session_free(session);
    close(sock);
    fprintf(stderr, "all done\n"); 
    libssh2_exit(); 
    return 0;
}
    
static void* wrapper_sendcmd(void *_dd)
{
    struct sendcmddata *dd = (struct sendcmddata*) _dd;
    _send_cmd(dd->host, dd->user, dd->pw, dd->cmd, dd->ss);
}

sshwrapper::sshwrapper() 
{    

}

sshwrapper::~sshwrapper() {
}

int sshwrapper::send_cmd_nonblk(std::string& host, std::string& user, std::string& pw,const  char *cmd, std::string& ss)
{
    struct sendcmddata dd;
    dd.host = host;
    dd.user = user;
    dd.pw = pw;
    dd.cmd = cmd;
    dd.ss = ss;
    pthread_t worker_thread;
    pthread_create(&worker_thread, NULL, wrapper_sendcmd, &dd );
    
}

int sshwrapper::send_cmd(std::string& host, std::string& user, std::string& pw,const  char *cmd, std::string& ss)
{
    return _send_cmd(host,user,pw, cmd,ss);
}
    

    

/**
 * @param host
 * @param user
 * @param pw
 * @param src
 * @param dest
 * @param ss
 * @return 
 * 
 * http://www.libssh2.org/examples/scp_write.html
 */
int sshwrapper::scp_write(std::string& host, std::string& user, std::string& pw, std::string& src, std::string& dest)
{
     unsigned long hostaddr;
    int sock, i, auth_pw = 1;
    struct sockaddr_in sin;
    const char *fingerprint;
    LIBSSH2_SESSION *session = NULL;
    LIBSSH2_CHANNEL *channel;
    FILE *local;
    int rc;
    char mem[1024];
    size_t nread;
    char *ptr;
    struct stat fileinfo;
    
    hostaddr = inet_addr(host.c_str()); 
    rc = libssh2_init (0);
    if (rc != 0) {
        fprintf (stderr, "libssh2 initialization failed (%d)\n", rc);
        return 1;
    }
 
    local = fopen(src.c_str(), "rb");
    if (!local) {
        fprintf(stderr, "Can't open local file %s\n", src.c_str());
        return -1;
    } 
    stat(src.c_str(), &fileinfo);
 
    /* Ultra basic "connect to port 22 on localhost"
     * Your code is responsible for creating the socket establishing the
     * connection
     */ 
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == sock) {
        fprintf(stderr, "failed to create socket!\n");
        return -1;
    }
 
    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = hostaddr;
    if (connect(sock, (struct sockaddr*)(&sin),
            sizeof(struct sockaddr_in)) != 0) {
        fprintf(stderr, "failed to connect!\n");
        return -1;
    }
 
    /* Create a session instance
     */ 
    session = libssh2_session_init();

    if(!session)
        return -1;
 
    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */ 
    rc = libssh2_session_handshake(session, sock);
    if(rc) {
        fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
        return -1;
    }
 
    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */ 
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    /*fprintf(stderr, "Fingerprint: ");
    for(i = 0; i < 20; i++) {
        fprintf(stderr, "%02X ", (unsigned char)fingerprint[i]);
    }
    fprintf(stderr, "\n");
    **/
    if (auth_pw) {
        /* We could authenticate via password */ 
        if (libssh2_userauth_password(session, user.c_str(), pw.c_str())) {

            fprintf(stderr, "Authentication by password failed.\n");
            goto shutdown;
        }
    }
 
    /* Send a file via scp. The mode parameter must only have permissions! */ 
    channel = libssh2_scp_send(session, dest.c_str(), fileinfo.st_mode & 0777,(unsigned long)fileinfo.st_size);
 
    if (!channel) {
        char *errmsg;
        int errlen;
        int err = libssh2_session_last_error(session, &errmsg, &errlen, 0);
        fprintf(stderr, "Unable to open a session: (%d) %s\n", err, errmsg);
        goto shutdown;
    }
 
    //fprintf(stderr, "SCP session waiting to send file\n");
    do {
        nread = fread(mem, 1, sizeof(mem), local);
        if (nread <= 0) {
            /* end of file */ 
            break;
        }
        ptr = mem;
 
        do {
            /* write the same data over and over, until error or completion */ 
            rc = libssh2_channel_write(channel, ptr, nread);

            if (rc < 0) {
                fprintf(stderr, "ERROR %d\n", rc);
                break;
            }
            else {
                /* rc indicates how many bytes were written this time */ 
                ptr += rc;
                nread -= rc;
            }
        } while (nread);
 
    } while (1);
 
   // fprintf(stderr, "Sending EOF\n");
    libssh2_channel_send_eof(channel);
  //  fprintf(stderr, "Waiting for EOF\n");
    libssh2_channel_wait_eof(channel);
//    fprintf(stderr, "Waiting for channel to close\n");
    libssh2_channel_wait_closed(channel);
    libssh2_channel_free(channel);
    channel = NULL;
 
 shutdown:
    if(session) {
        libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
        libssh2_session_free(session);
    }
 
    close(sock);
    if (local)
        fclose(local);
 
    libssh2_exit();
    return 0;
}
