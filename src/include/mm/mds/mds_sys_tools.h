#ifndef MDS_SYS_TOOLS_H_
#define MDS_SYS_TOOLS_H_

#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <stdlib.h>


#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

bool systools_fs_mkdir(std::string file);
bool systools_fs_cpfile(std::string src,std::string dest,bool force);
bool systools_fs_chmod(std::string dest, std::string mode);

#endif
