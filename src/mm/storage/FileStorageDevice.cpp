/*
 * FileStorageDevice.cpp
 *
 *  Created on: 12.04.2011
 *      Author: ck
 */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <list>
#include <sys/timeb.h>
#include <string>

#include "mm/storage/storage.h"
#include "global_types.h"


FileStorageDevice::FileStorageDevice(char *path)
{
    this->profiler = Pc2fsProfiler::get_instance();
    this->identifier = path;
}


char *FileStorageDevice::get_identifier()
{
    return this->identifier;
}

void FileStorageDevice::set_identifier(char *identifier)
{
    this->identifier = identifier;
}

void FileStorageDevice::read_object(const char *identifier, off_t offset, size_t length, void *data)
{
    #ifdef IO_PROFILING 
    timeb before;	
    ftime(&before);
    #endif  
    int fh;
    char filename[MAX_NAME_LEN];
    profiler->function_start();

    snprintf(filename, MAX_NAME_LEN, "%s/%s", this->get_identifier(), identifier);

    if((fh = open(filename, O_RDONLY)) != -1)
    {
        size_t count;
        profiler->function_sleep();
        count = pread(fh, data, length, offset);
        profiler->function_wakeup();
   
	close(fh);
        #ifdef IO_PROFILING 
        timeb after;
        ftime(&after);

        
        printf("Reading an object took %d seconds and  %u miliseconds \n", after.time - before.time, after.millitm - before.millitm);
	#endif 
	
	if(count == length)
        {
        	profiler->function_end();
            return;
        }
    }

   profiler->function_end();
    throw StorageException("Error while reading object");
}

void FileStorageDevice::write_object(const char *identifier, off_t offset, size_t length, void *data)
{
	this->write_object(identifier, offset, length, data, false);
}

void FileStorageDevice::write_object(const char *identifier, off_t offset, size_t length, void *data, bool no_sync)
{
    #ifdef IO_PROFILING 
    timeb before;
    timeb after;
    ftime(&before);
    #endif  
    int fh;
    char filename[MAX_NAME_LEN];
    size_t count;

    profiler->function_start();

    snprintf(filename, MAX_NAME_LEN, "%s/%s", this->get_identifier(), identifier);
    
    fh = open(filename, O_WRONLY);

    #ifdef IO_PROFILING 
    ftime(&after);
    printf("Opening the object took %d seconds and  %u miliseconds \n", after.time - before.time, after.millitm - before.millitm);
    ftime(&before);    
    #endif

    if(fh == -1)
    {
    	profiler->function_sleep();
        fh = creat(filename, 0600);
        profiler->function_wakeup();
    }
 
    #ifdef IO_PROFILING 
    ftime(&after);
    printf("Creating the object took %d seconds and  %u miliseconds \n", after.time - before.time, after.millitm - before.millitm);
    ftime(&before);    
    #endif

    profiler->function_sleep();
    count = pwrite(fh, data, length, offset);

     
    #ifdef IO_PROFILING 
    ftime(&after);
    printf("Writing the object took %d seconds and  %u miliseconds \n", after.time - before.time, after.millitm - before.millitm);
    ftime(&before);
    #endif

    if(!no_sync)
    	fsync(fh);
    
    #ifdef IO_PROFILING 
    ftime(&after);
    printf("Syncing an object took %d seconds and  %u miliseconds \n", after.time - before.time, after.millitm - before.millitm);
    ftime(&before);
    #endif
    
    close(fh);

 
    #ifdef IO_PROFILING 
    ftime(&after);
    printf("Closing the FH  took %d seconds and  %u miliseconds \n", after.time - before.time, after.millitm - before.millitm);
    ftime(&before);
    #endif


    profiler->function_wakeup();
   
    profiler->function_end();
    if(count == length)
        return;
    throw StorageException("Error while writing object");
}

void FileStorageDevice::truncate_object(const char *identifier, off_t length)
{
    char filename[MAX_NAME_LEN];
    profiler->function_start();

    snprintf(filename, MAX_NAME_LEN, "%s/%s", this->get_identifier(), identifier);
    profiler->function_sleep();
    if (truncate(filename, length) == -1)
    {
    	profiler->function_wakeup();
    	profiler->function_end();
        throw StorageException("Error while truncating object");
    }
	profiler->function_wakeup();
	profiler->function_end();
}

size_t FileStorageDevice::get_object_size(const char *identifier)
{
    char filename[MAX_NAME_LEN];
    struct stat st;

    snprintf(filename, MAX_NAME_LEN, "%s/%s", this->get_identifier(), identifier);
    if(stat(filename, &st) == -1)
    {
        return 0;
    }
    return st.st_size;
}

void FileStorageDevice::remove_object(const char* identifier)
{
    char filename[MAX_NAME_LEN];
    profiler->function_start();

    snprintf(filename, MAX_NAME_LEN, "%s/%s", this->get_identifier(), identifier);
    profiler->function_sleep();
    if(remove(filename) == -1)
    {
    	profiler->function_wakeup();
    	profiler->function_end();
        throw StorageException("Error while removing object");
    }
	profiler->function_wakeup();
	profiler->function_end();
}

std::list<std::string> *FileStorageDevice::list_objects()
{
	profiler->function_start();
    std::list<std::string> *result = new std::list<std::string>();
    struct dirent *entry;
    DIR *dirp;

    profiler->function_sleep();
    if ((dirp = opendir(this->identifier)) == NULL)
    {
    	profiler->function_wakeup();
    	profiler->function_end();
        throw StorageException("Cannot read objects");
    }

    profiler->function_sleep();
    while ((entry = readdir(dirp)) != NULL)
    {
    	profiler->function_wakeup();
        std::string s = entry->d_name;
        if (s != ".." && s != ".")
            result->push_back(s);
    }

    free(dirp);
    profiler->function_end();
    return result;
}

bool FileStorageDevice::has_object(const char* identifier)
{
    char filename[MAX_NAME_LEN];
    profiler->function_start();

    snprintf(filename, MAX_NAME_LEN, "%s/%s", this->get_identifier(), identifier);

    profiler->function_sleep();
    if(open(filename, O_RDONLY) == -1)
    {
    	profiler->function_wakeup();
    	profiler->function_end();
    	return false;
    }
    else
    {
    	profiler->function_wakeup();
    	profiler->function_end();
        return true;
    }
}
