#ifndef MDS_SYS_TOOLS_H_
#define MDS_SYS_TOOLS_H_

#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>


struct timertimes {
    long usec;
    long sec;
};


void xsystools_fs_flush(std::string dir);
bool xsystools_fs_mkdir(std::string file);
bool xsystools_fs_cpfile(std::string src,std::string dest,bool force);
bool xsystools_fs_chmod(std::string dest, std::string mode);

inline void myusleep(int millisec);

double timer_end(struct timertimes tts);
struct timertimes  timer_start() ;

void flush_sysbuffer();
void daemonize(void);

#endif

