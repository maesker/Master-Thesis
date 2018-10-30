/*
 * A client to the pc2mds which simulates the behaviour 
 * of ganesha. 
 *
 * Written by Sebastian Moors <mauser@smoors.de> (2011)
 * (Some of the openmpi code was taken directly from metarates.c) 
 */

#include <stdio.h>
#include <stdlib.h>

#include "mpi.h"
#include "zmq.h"

#include  "fsal_zmq_client_library.h"
#include "fakeClient.h"
#include "global_types.h"
#include "communication.h"

#include "message_types.h"

struct socket_array_entry_t* socket_entry;

/*some helper methods from metarates */

static long initialsec = 0;
static long initialusec = 0;

int log_step_size = 10000;

int initsec() {
	struct timeval tv;
	int ival;

	ival = gettimeofday(&tv, NULL);
        if (ival != 0) {
		fprintf(stderr, "error in call to gettimeofday!\n");
		exit(1);
	}

	initialsec  = tv.tv_sec;
	initialusec = tv.tv_usec;

	return 0;

}

double secondr()
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

        if (tv.tv_usec < initialusec) {
		tv.tv_usec += 1000000;
                tv.tv_sec -= 1;
        }
	diffs  = tv.tv_sec - initialsec;
        diffus = tv.tv_usec - initialusec;

        diff = (double)(diffs + (diffus / 1000000.));

	return (diff);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/*
// Create a socket for the communication with ganesha 
void setup_socket()
{
	    socket_entry = (struct socket_array_entry_t*) malloc(sizeof(struct socket_array_entry_t));
 
	    void *context = zmq_init(1);
            void *socket = zmq_socket(context, ZMQ_REQ);
            zmq_connect (socket, ZMQ_ROUTER_FRONTEND );

            socket_entry->zmq_socket = socket;
            socket_entry->zmq_context = context;
            socket_entry->zmq_request_mem = malloc(FSAL_MSG_LEN);
}


// Release the socket
void teardown_socket()
{
	free(socket_entry->zmq_request_mem);
	free(socket_entry);
}


// Return the socket
void* get_socket()
{
                      return socket_entry;
}*/



/*
 * This methods mimics a typical command flow which happens if 
 * ganesha changes the time attributes of a file.
 *
 * Commands:
 *
 * Lookup filename
 * Get Einode
 * Set Attributes
 * Get Einode
 *
 */

int combined_utime(InodeNumber root_inode, InodeNumber parent_inode, char* p_filename)
{

	InodeNumber return_ino;

    	inode_create_attributes_t attrs;
    	attrs.mode = 0777;
    	attrs.size = 0;
    	strncpy(attrs.name, p_filename, MAX_NAME_LEN);
    	attrs.name[strlen(attrs.name)] = '\0';
    	attrs.uid = 0;
    	attrs.gid = 0;


	//Make  lookups to check if the file exists. 

	int rc = fsal_lookup_inode( &root_inode, &parent_inode, p_filename, &return_ino);

	if(rc != 0)
	{
		//file is not existing
		printf("File %s is not existing, aborting utime.\n", p_filename);
		return -1;	
	}
	

	 //Get the einode (attributes)
  	struct EInode einode;
	rc = fsal_read_einode( &root_inode , &return_ino, &einode );
	
	if(rc != 0)
	{
		printf("Getting attributes for the file %s failed, aborting set utime.\n", p_filename);
		return -1;	
	}

	 //Simulate the time update..

	int bitfield = 0; 
	SET_ATIME(bitfield);
      	SET_MTIME(bitfield);
	
  	inode_update_attributes_t update_attrs;
       	update_attrs.atime = update_attrs.mtime = time(NULL); 
	rc = fsal_update_attributes(&root_inode, &return_ino, &update_attrs, &bitfield);
	
	if(rc != 0)
	{
		printf("Updating attributes for the file %s failed, aborting utime.\n", p_filename);
		return -1;	
	}


	 //Get the einode (attributes)
	rc = fsal_read_einode( &root_inode , &return_ino, &einode );
	
	if(rc != 0)
	{
		printf("Getting attributes for the file %s failed, aborting utime.\n", p_filename);
		return -1;	
	}

	return 0;

}






/*
 * This methods mimics a typical command flow which happens if 
 * ganesha creates a file.
 *
 * Commands:
 *
 * Lookup filename
 * Lookup filename
 * Create filename
 * Get Einode
 * Get Einode
 * Set Attributes
 * Get Einode
 *
 */

int combined_create(InodeNumber root_inode, InodeNumber parent_inode, char* p_filename)
{

	InodeNumber return_ino;

    	inode_create_attributes_t attrs;
    	attrs.mode = 0777;
    	attrs.size = 0;
    	strncpy(attrs.name, p_filename, MAX_NAME_LEN);
    	attrs.name[strlen(attrs.name)] = '\0';
    	attrs.uid = 0;
    	attrs.gid = 0;


	//Make two lookups to check if the file exists. Don't ask me why *two*.

	int rc = fsal_lookup_inode( &root_inode, &parent_inode, p_filename, &return_ino);

	if(rc == 0)
	{
		//file is already existing
		printf("File %s is already existing, aborting create.\n", p_filename);
		return -1;	
	}
	
	rc = fsal_lookup_inode( &root_inode, &parent_inode, p_filename, &return_ino);

	if(rc == 0)
	{
		//file is already existing
		printf("File %s is already existing, aborting create.\n", p_filename);
		return -1;	
	}


	//Create the file
	rc = fsal_create_file_einode( &root_inode , &parent_inode, &attrs , &return_ino);

	if(rc != 0)
	{
		printf("Creating the file %s failed, aborting create.\n", p_filename);
		return -1;	
	}

	 //Get the einode (attributes)
  	struct EInode einode;
	rc = fsal_read_einode( &root_inode , &return_ino, &einode );
	
	if(rc != 0)
	{
		printf("Getting attributes for the file %s failed, aborting create.\n", p_filename);
		return -1;	
	}

	 //Get the einode (attributes)
	rc = fsal_read_einode( &root_inode , &return_ino, &einode );
	
	if(rc != 0)
	{
		printf("Getting attributes for the file %s failed, aborting create.\n", p_filename);
		return -1;	
	}


	 //Simulate the time update..

	int bitfield = 0; 
	SET_ATIME(bitfield);
      	SET_MTIME(bitfield);
	
  	inode_update_attributes_t update_attrs;
       	update_attrs.atime = update_attrs.mtime = time(NULL); 
	rc = fsal_update_attributes(&root_inode, &return_ino, &update_attrs, &bitfield);
	
	if(rc != 0)
	{
		printf("Updating attributes for the file %s failed, aborting create.\n", p_filename);
		return -1;	
	}


	 //Get the einode (attributes)
	rc = fsal_read_einode( &root_inode , &return_ino, &einode );
	
	if(rc != 0)
	{
		printf("Getting attributes for the file %s failed, aborting create.\n", p_filename);
		return -1;	
	}

	return 0;

}

/*
* This methods mimics a typical command flow which happens if 
* ganesha stat's a file.
*
* Commands:
*
* Lookup filename
* Get Einode for inode_number
**/


int combined_stat(InodeNumber root_inode, InodeNumber parent_inode, char* p_filename)
{

	InodeNumber return_ino;

	//Make two lookups to check if the file exists. Don't ask me why *two*.

	int rc = fsal_lookup_inode( &root_inode, &parent_inode, p_filename, &return_ino);

	if(rc != 0)
	{
		//file is not existing
		printf("File %s is not existing, aborting stat.\n", p_filename);
		return -1;	
	}
	

	 //Get the einode (attributes)
  	struct EInode einode;
	rc = fsal_read_einode( &root_inode , &root_inode, &einode );
	
	if(rc != 0)
	{
		printf("Getting attributes for the root inode failed, aborting stat.\n");
		return -1;	
	}

	return 0;

}

/*
* This methods mimics a typical command flow which happens if 
* ganesha creates a file.
*
* Commands:
*
* Lookup filename
* Delete filename
* Get Einode for parent
**/


int combined_delete(InodeNumber root_inode, InodeNumber parent_inode, char* p_filename)
{

	InodeNumber return_ino;

	//Make two lookups to check if the file exists. Don't ask me why *two*.

	int rc = fsal_lookup_inode( &root_inode, &parent_inode, p_filename, &return_ino);

	if(rc != 0)
	{
		//file is already existing
		printf("File %s is not existing, aborting delete.\n", p_filename);
		return -1;	
	}
	


	//Delete the file
  	rc = fsal_delete_einode( &root_inode, &return_ino);

	if(rc != 0)
	{
		printf("Deleting the file %s failed, aborting delete.\n", p_filename);
		return -1;	
	}

	 //Get the einode (attributes)
  	struct EInode einode;
	rc = fsal_read_einode( &root_inode , &root_inode, &einode );
	
	if(rc != 0)
	{
		printf("Getting attributes for the root inode failed, aborting delete.\n");
		return -1;	
	}

	return 0;

}

/*
 * The combined_readdir method simulates a readdir from ganesha.
 * This includes a readdir for n entries with getattr operations for
 * all entries.
 */

int combined_readdir(InodeNumber root_inode, InodeNumber directory)
{
  	struct ReadDirReturn read_dir;
	struct EInode einode;
	int number_of_entries = 0;
  	int i;
	int rc=0;
	off_t offset=0;

	int files_per_readdir=1;

	double last_time = secondr();
	double diff_time = 0.0;

	//make sure this is dividable by files_per_readdir
	unsigned int step_size = log_step_size;

	rc = fsal_read_dir(&root_inode,&directory , &offset , &read_dir);
	
	if(rc != 0)
	{
		printf("Readdir failed with status %d, aborting.\n",  rc );
		return -1;	
	}


	while( offset + read_dir.nodes_len < read_dir.dir_size )
	{ 
		for(i = 0; i < read_dir.nodes_len; i++)
		{

			if( strlen( read_dir.nodes[i].name ) > 0  )
			{
				if((offset + 2)  % step_size == 0 && i == 0)
				{
					diff_time = secondr() - last_time;	
					printf("%u;%.4f;%.4f \n",offset,diff_time, step_size / (float) diff_time); 	
					last_time = secondr();
				}

				rc = fsal_read_einode( &root_inode , &read_dir.nodes[i].inode.inode_number, &einode );
		
				if(rc != 0)
				{
					printf("Readdir: Getting attributes for the file %s failed, aborting create.\n",  read_dir.nodes[i].name );
					return -1;	
				}

			}
		}
		
		offset = offset + files_per_readdir;
		rc = fsal_read_dir(&root_inode,&directory , &offset , &read_dir);
	
		if(rc != 0)
		{
			printf("Readdir failed with status %d, aborting.\n",  rc );
			return -1;	
		}

	}
	return 0;
}

/*
 * Measures the creation of files
 * Looks for the files 0 - number_of_files and creates them in 
 * directory identified by parent_inode and root_inode.
 */

int measureCreates(InodeNumber root_inode, InodeNumber parent_inode, int number_of_files, char* p_basename, int node, int npes)
{
	/*
	 * The filenames look like this: servername-0 , servername-1 ... 
	 * 254 bytes for a string representation of a number should be sufficient + 1024 bytes for the hostname
	 */
	
	double		time0, time1, ctime, ctmax, ctmin, ctsum, ctavg, cttot;
	char* p_filename = malloc(sizeof(char)*(1024));
	
	memset(p_filename,' ', 1024);	

	unsigned int before = time(NULL);

	double last_time = secondr();
	double diff_time = 0.0;


	int i;
	int rc = 0;

	int ival  = initsec();
	MPI_Barrier(MPI_COMM_WORLD);
	time0 = secondr();


	for(i=2; i < number_of_files; i++)
	{
		//constructing the filename
		sprintf( p_filename, "%s-p%d-f%d", p_basename, node, i);

		rc = combined_create(root_inode,parent_inode,p_filename);
		
		if(rc != 0)
		{
			printf("Creating file '%s' on host %s with process number %d failed!\n", p_filename, p_basename, node);
			return -1;
		}

		memset(p_filename,' ', 1024);
		
		if( i % log_step_size == 0)
		{
			diff_time = secondr() - last_time;	
			printf("%u;%.4f;%.4f \n",i,diff_time, log_step_size / (float) diff_time); 	
			last_time = secondr();
		}

	
	}

	time1 = secondr();
	ctime = time1 - time0;
   

	unsigned int after = time(NULL);


	MPI_Reduce(&ctime, &ctmax, 1, MPI_DOUBLE, MPI_MAX, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctmin, 1, MPI_DOUBLE, MPI_MIN, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctsum, 1, MPI_DOUBLE, MPI_SUM, 0, 
				MPI_COMM_WORLD);
   
	int numfilesPerProc = number_of_files;

	if (node == 0) {
		ctmin = (double)(numfilesPerProc)/ctmin;
		ctmax = (double)(numfilesPerProc)/ctmax;
		ctavg = ((double)numfilesPerProc)*npes/ctsum;
		cttot = ctmax*npes;
		printf("\nrates at which files can be created "
				"and closed with %d pes:\n", npes);
		
		printf("max rate = %f creates and closes per "
				"second, min rate = %f creates and "
				"closes per second, avg rate = %f "
				"creates and closes per second, total "
				"rate = %f creates and closes per "
				"second\n", ctmin, ctmax, ctavg, cttot);
    	}

	
	free(p_filename);
}

int measureUtimes(InodeNumber root_inode, InodeNumber parent_inode, int number_of_files, char* p_basename, int node, int npes)
{
	/*
	 * The filenames look like this: servername-0 , servername-1 ... 
	 * 254 bytes for a string representation of a number should be sufficient + 1024 bytes for the hostname
	 */
	
	double		time0, time1, ctime, ctmax, ctmin, ctsum, ctavg, cttot;
	char* p_filename = malloc(sizeof(char)*(1024));
	
	memset(p_filename,' ', 1024);	

	unsigned int before = time(NULL);

	double last_time = secondr();
	double diff_time = 0.0;


	int i;
	int rc = 0;

	int ival  = initsec();
	MPI_Barrier(MPI_COMM_WORLD);
	time0 = secondr();


	for(i=2; i < number_of_files; i++)
	{
		//constructing the filename
		sprintf( p_filename, "%s-p%d-f%d", p_basename, node, i);

		rc = combined_utime(root_inode,parent_inode,p_filename);
		
		if(rc != 0)
		{
			printf("Set utime on file '%s' on host %s with process number %d failed!\n", p_filename, p_basename, node);
			return -1;
		}

		memset(p_filename,' ', 1024);
		
		if( i % log_step_size == 0)
		{
			diff_time = secondr() - last_time;	
			printf("%u;%.4f;%.4f \n",i,diff_time, log_step_size / (float) diff_time); 	
			last_time = secondr();
		}

	
	}

	time1 = secondr();
	ctime = time1 - time0;
   

	unsigned int after = time(NULL);


	MPI_Reduce(&ctime, &ctmax, 1, MPI_DOUBLE, MPI_MAX, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctmin, 1, MPI_DOUBLE, MPI_MIN, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctsum, 1, MPI_DOUBLE, MPI_SUM, 0, 
				MPI_COMM_WORLD);
   
	int numfilesPerProc = number_of_files;

	if (node == 0) {
		ctmin = (double)(numfilesPerProc)/ctmin;
		ctmax = (double)(numfilesPerProc)/ctmax;
		ctavg = ((double)numfilesPerProc)*npes/ctsum;
		cttot = ctmax*npes;
		printf("\nrates at which files can be utimed' "
				"with %d pes:\n", npes);
		
		printf("max rate = %f utimes per "
				"second, min rate = %f "
				"utimes per second, avg rate = %f "
				"utimes per second, total "
				"rate = %f utimes per "
				"second\n", ctmin, ctmax, ctavg, cttot);
    	}

	
	free(p_filename);
}

int measureStats(InodeNumber root_inode, InodeNumber parent_inode, int number_of_files, char* p_basename, int node, int npes)
{
	/*
	 * The filenames look like this: servername-0 , servername-1 ... 
	 * 254 bytes for a string representation of a number should be sufficient + 1024 bytes for the hostname
	 */
	
	double		time0, time1, ctime, ctmax, ctmin, ctsum, ctavg, cttot;
	char* p_filename = malloc(sizeof(char)*(1024));
	
	memset(p_filename,' ', 1024);	

	unsigned int before = time(NULL);

	double last_time = secondr();
	double diff_time = 0.0;


	int i;
	int rc = 0;

	int ival  = initsec();
	MPI_Barrier(MPI_COMM_WORLD);
	time0 = secondr();


	for(i=2; i < number_of_files; i++)
	{
		//constructing the filename
		sprintf( p_filename, "%s-p%d-f%d", p_basename, node, i);

		rc = combined_stat(root_inode,parent_inode,p_filename);
		
		if(rc != 0)
		{
			printf("Performing stat on file '%s' on host %s with process number %d failed!\n", p_filename, p_basename, node);
			return -1;
		}

		memset(p_filename,' ', 1024);
		
		if( i % log_step_size == 0)
		{
			diff_time = secondr() - last_time;	
			printf("%u;%.4f;%.4f \n",i,diff_time, log_step_size / (float) diff_time); 	
			last_time = secondr();
		}

	
	}

	time1 = secondr();
	ctime = time1 - time0;
   

	unsigned int after = time(NULL);


	MPI_Reduce(&ctime, &ctmax, 1, MPI_DOUBLE, MPI_MAX, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctmin, 1, MPI_DOUBLE, MPI_MIN, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctsum, 1, MPI_DOUBLE, MPI_SUM, 0, 
				MPI_COMM_WORLD);
   
	int numfilesPerProc = number_of_files;

	if (node == 0) {
		ctmin = (double)(numfilesPerProc)/ctmin;
		ctmax = (double)(numfilesPerProc)/ctmax;
		ctavg = ((double)numfilesPerProc)*npes/ctsum;
		cttot = ctmax*npes;
		printf("\nrates at which files can be stat'd "
				"with %d pes:\n", npes);
		
		printf("max rate = %f stats per "
				"second, min rate = %f stats"
				" per second, avg rate = %f "
				"stats per second, total "
				"rate = %f stats per "
				"second\n", ctmin, ctmax, ctavg, cttot);
    	}

	
	free(p_filename);
}

/*
 * Measures the removal of files.
 * Looks for the files 0 - number_of_files and deletes them in 
 * directory identified by parent_inode and root_inode.
 */

int measureDeletes(InodeNumber root_inode, InodeNumber parent_inode, int number_of_files, char* p_basename, int node, int npes)
{

	double		time0, time1, ctime, ctmax, ctmin, ctsum, ctavg, cttot, time3, time4;

	char* p_filename_number = malloc(sizeof(char)*(254));
	char* p_filename = malloc(sizeof(char)*(1024));
	memset(p_filename,' ', 1024);



	double diff_time = 0.0;



	int i;
	int rc = 0;

	int ival  = initsec();
	MPI_Barrier(MPI_COMM_WORLD);
	time0 = secondr();
	double last_time = secondr();

	for(i=2; i < number_of_files; i++)
	{
		//constructing the filename
		sprintf( p_filename, "%s-p%d-f%d", p_basename, node, i);

		rc = combined_delete(root_inode,parent_inode,p_filename);

		if(rc != 0)
		{
			printf("Deleting file '%s' on host %s with process number %d failed!\n", p_filename, p_basename, node);
			return -1;
		}

		memset(p_filename,' ', 1024);
		
		if( i % log_step_size == 0)
		{
			diff_time = secondr() - last_time;	
			printf("%u;%.4f;%.4f \n",i,diff_time, log_step_size / (float) diff_time); 	
			last_time = secondr();
		}

	
	}
 
	time1 = secondr();
	ctime = time1 - time0;
   



	MPI_Reduce(&ctime, &ctmax, 1, MPI_DOUBLE, MPI_MAX, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctmin, 1, MPI_DOUBLE, MPI_MIN, 0, 
				MPI_COMM_WORLD);
	MPI_Reduce(&ctime, &ctsum, 1, MPI_DOUBLE, MPI_SUM, 0, 
				MPI_COMM_WORLD);
   
	int numfilesPerProc = number_of_files;

	if (node == 0) {
		ctmin = (double)(numfilesPerProc)/ctmin;
		ctmax = (double)(numfilesPerProc)/ctmax;
		ctavg = ((double)numfilesPerProc)*npes/ctsum;
		cttot = ctmax*npes;
		printf("\nrates at which files can be deleted "
				" with %d pes:\n", npes);
		
		printf("max rate = %f deletes per "
				"second, min rate = %f deletes"
				" per second, avg rate = %f "
				"deletes per second, total "
				"rate = %f deletes per "
				"second\n", ctmin, ctmax, ctavg, cttot);
    	}

	
	free(p_filename);
}

/*
 * Measures the performance of a readdir operation, issued on inode
 * directory_inode and the partition with the corresponding root_inode
 */

int measureReaddir(InodeNumber root_inode, InodeNumber directory_inode)
{

	double before = secondr();
	int rc = 0;
	
	rc = combined_readdir(root_inode,directory_inode);

	if(rc != 0)
	{
		printf("Performing readdir failed!\n");
		return -1;
	}


	double after = secondr();
	
	printf("Reading a directory took %f seconds \n", after - before);
	return 0;
}


void showHelp()
{
	printf("Welcome to the fakeClient by Sebastian Moors <mauser@smoors.de> \n");
	printf("This is an openmpi-enabled program. Use mpirun.openmpi to run it!\n");
	printf("Options: \n");
	printf("\t -n Determine number of files \n");
	printf("\t -c Create files \n");
	printf("\t -r Perform readdir \n");
	printf("\t -d Delete files \n");
	printf("\t -s Do stats\n");
	printf("\t -u Do utime \n");
}


int main (int argc, char* argv[]) 
{
	/* MPI Initialisation */

	int node, npe;
	
	
	//0: Every process writes in its own directory
	//1: One dir for all processes
	int shared;

	//Boolean switches for the benchmark modes
	int create, delete, readdir, stats, utime;

	//number of files to create or delete
	int number_of_files;

	//return value for benchmark operations
	int rc = 0;

	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD,&npe);
	MPI_Comm_rank(MPI_COMM_WORLD,&node);

	/* Creates a ZMQ socket for compatibility with the 
	 * functions from our zmqclient library
	 */
	setup_socket();


	/*
	 * get input parameters
	 */	
	char opts[] = "n:cdrsu";
	int c;

	if ( argc <= 2 ) {
		showHelp();
		exit(1);
	}
  
	while ( ( c = getopt(argc,argv,opts) ) != -1 ) {
		switch (c) {
		case 'n':
			number_of_files = atoi(optarg);
			break;
		case 's':
			stats = 1;
			break;
		case 'u':
			utime = 1;
			break;
		case 'c':
			create = 1;
			break;
		case 'd':
			delete = 1;
			break;
		case 'r':
			readdir = 1;
			break;
		case '?':
		case ':':
			showHelp();
			exit(4);
		}
	}





	//Print the hostnames and ranks
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	printf("Hostname: %s has rank %d \n", hostname, node);


	if(node == 0)
	{
		printf("Welcome to the fakeClient benchmarking & testing utility.\n");
		printf("The following testrun will create %u files\n", number_of_files );
	}


	/* Perform the benchmark */
	if(create == 1){
		rc = measureCreates(1,1, number_of_files, hostname, node, npe);
		if(rc == -1){
			MPI_Finalize();
			exit(1);
		}
	}


	if(readdir == 1)
	{
		rc = measureReaddir(1,1);
	}
	
	if(stats == 1)
	{
		rc = measureStats(1,1,number_of_files,hostname,node,npe);
	}

	if(utime == 1)
	{
		rc = measureUtimes(1,1,number_of_files,hostname,node,npe);
	}

	if(delete == 1)
	{
		rc = measureDeletes(1,1, number_of_files, hostname, node, npe);
		MPI_Finalize();
		exit(1);
	}

	/* Delete the socket and exit */

	teardown_socket();
	MPI_Finalize();
	return 0;
}
