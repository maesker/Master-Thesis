#include "mm/mds/mds_sys_tools.h"

bool systools_fs_mkdir(std::string file)
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


bool systools_fs_cpfile(std::string src,std::string dest, bool force)
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


bool systools_fs_chmod(std::string dest, std::string mode)
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

