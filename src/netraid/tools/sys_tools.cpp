#include "tools/sys_tools.h"

bool xsystools_fs_mkdir(std::string file)
{
    bool ret=false;
    boost::filesystem::path p(file);
    if(boost::filesystem::exists(p))
    {
        ret = true;
    }
    else
    {
        std::string cmd_mktmp = std::string("mkdir ");
        cmd_mktmp += file;
        system(cmd_mktmp.c_str());
        ret = true;
    }
    return ret;
}


bool xsystools_fs_cpfile(std::string src,std::string dest, bool force)
{
    bool ret=false;
    boost::filesystem::path p(dest);
    if(boost::filesystem::exists(p) && !force)
    {
        ret = false;
    }
    else
    {
        std::string cmd_cp = std::string("cp ");
        cmd_cp += src ;
        cmd_cp += " " ;
        cmd_cp += dest ;
        //printf("%s\n",cmd_cp.c_str());
        system(cmd_cp.c_str());
        ret = true;
    }
    return ret;
}

bool xsystools_fs_chmod(std::string dest, std::string mode)
{
    bool ret=false;
    boost::filesystem::path p(dest);
    if(boost::filesystem::exists(p))
    {
        std::string cmd = std::string("chmod -R ");
        cmd += mode + " " + dest;
        system(cmd.c_str());
        ret = true;
    }
    else
    {
        ret = false;
    }
    return ret;
}

void xsystools_fs_flush(std::string dir)
{
    char *p = (char*) malloc(1024);
    if (dir[0]=='/' && dir.size()>2)
    {
        sprintf(p,"rm %s/*",dir.c_str());
        system(p);
    }
}

inline void myusleep(int millisec){  usleep(millisec*1000); }


struct timertimes  timer_start() 
{
        struct timertimes tts;
	struct timeval tv;
	int ival;

	ival = gettimeofday(&tv, NULL);
        if (ival != 0) {
		fprintf(stderr, "error in call to gettimeofday!\n");
		exit(1);
	}
	tts.sec  = tv.tv_sec;
	tts.usec = tv.tv_usec;
	return tts;
}

double timer_end(struct timertimes tts)
{
	struct timeval tv;
	int ival;
        double diff;
	long diffs, diffus;

	ival = gettimeofday(&tv, NULL);
        if (ival != 0) {
		fprintf(stderr, "error in call to gettimeofday!\n");
		exit(1);
        }
        if (tv.tv_usec < tts.usec) {
		tv.tv_usec += 1000000;
                tv.tv_sec -= 1;
        }

      	diffs  = tv.tv_sec - tts.sec;
        diffus = tv.tv_usec - tts.usec;
        diff = (double)(diffs + (diffus / 1000000.0));
        return diff;
}


void flush_sysbuffer()
{
    system ("echo 3 | tee /proc/sys/vm/drop_caches "); // flush system buffer
}


#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

void daemonize(void)
{
    pid_t pid, sid;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* At this point we are executing as the child process */
    /* Change the file mode mask **/
    umask(0);

    /* Create a new SID for the child process 
    sid = setsid();**/
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. **/
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null 
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);**/
}
