/* 
 * File:   sshwrapper.h
 * Author: markus
 *
 * Created on 14. Juni 2012, 19:58
 */
 
#ifndef SSHWRAPPER_H
#define	SSHWRAPPER_H

#include "libssh2_config.h"
#include <libssh2.h>
 
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
 
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <string>


struct sendcmddata
{
    std::string host;
    std::string user;
    std::string pw;
    const char *cmd;
    std::string ss;
};

class sshwrapper {
public:
    sshwrapper();
    ~sshwrapper();
    int send_cmd_nonblk(std::string& host, std::string& user, std::string& pw,const  char *cmd, std::string& ss);
    int send_cmd(std::string& host, std::string& user, std::string& pw, const char *cmd, std::string& ss);
    int scp_write(std::string& host, std::string& user, std::string& pw, std::string& src, std::string& dest);
    
private:
    unsigned long hostaddr;
    int sock;
    struct sockaddr_in sin;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
};

#endif	/* SSHWRAPPER_H */

