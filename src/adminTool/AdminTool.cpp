/*
 * adminTool.cpp
 *
 * Sebastian Moors <mauser@smoors.de>
 *
 * This program can be used to add an MDS to an existing cluster or
 * to initiate a new cluster.
 */

#include <stdio.h>
#include <string.h>

#include "AdminOperations.h"


void print_unkown_parameter()
{
	printf("Unkown parameter. Use 'adminTool -h to see the possible parameters.\n");
}

void print_illegal_parameters()
{
	printf("Not enough parameters. Use 'adminTool -h to see the possible parameeters.\n");
}

void print_help()
{
	printf("Parameters: \n \t\t -h, --help \t\t\t Display this help information\n"
			"\t\t --add-mds ip:port  \t\t add MDS ip:port to local MLT \n" 
			"\t\t --remove-mds ip:port  \t\t remove MDS ip:port from local MLT \n" 
			"\t\t --synch-mlt \t\t\t copy local MLT to all servers listed in the MLT \n" 
			"\t\t --list-mds \t\t\t list servers found in MLT \n" 
			"\t\t --list-partitions \t\t list partitions managed by this MDS \n" 
			"\t\t --check-mds ip:port \t\t check MDS ip:port \n" 
			"\t\t --check-mds * \t\t\t check all MDS \n" 
			"\t\t --add-ds ip:port \t\t add DS ip:port \n" 
			"\t\t --delete-ds ip:port \t\t delete DS ip:port \n" 
			"\t\t --check-ds ip:port \t\t check  DS ip:port \n" 
			"\t\t --check-ds * \t\t\t check all DS \n" 
			"\t\t --move-tree ip:port path \t moves subtree represented by path to server ip:port \n" 
      			"\t\t -i,--init-mds ip:port \t\t initialise new MDS cluster\n "
			"\t\t --health \t\t\t print MDS + DS health report\n "
            		"\t\t --add-partition ip:port fullpath inodenumber\t Manually add a partition to the MLT.\n");
			
}


int main( int argc, char *argv[] )
{
	printf("Welcome to the MDS administration tool.\n");

	AdminOperations *adminOps = new AdminOperations();

	switch( argc )
	{
                /*
                 * We got 2 arguments. The first one is our filename, so we have just one parameter.
                 */

		case 2:
			if( strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0   ){
				print_help();
				break;
			}

			if( strcmp(argv[1], "--synch-mlt") == 0 ){
				adminOps->synch_mlt();
				break;
			} 

			if( strcmp(argv[1], "--list-mds") == 0 ){
				adminOps->list_mds();
				break;
			}

			if( strcmp(argv[1], "--list-partitions") == 0 ){
				adminOps->list_partitions();
				break;
			}
			
			print_unkown_parameter();
			break;


                /*
                 * Got 3 arguments => two parameters for us
                 */

		case 3:
                        if( strcmp(argv[1], "--init-mds") == 0   ){
                                adminOps->init_mds( argv[2] );
                                break;
                        }			
			
			if( strcmp(argv[1], "--remove-mds") == 0   ){
				adminOps->remove_mds( argv[2] );
				break;
			}

			if( strcmp(argv[1], "--add-mds") == 0   ){
				adminOps->add_mds( argv[2]);
				break;
			}
            
			
			print_unkown_parameter();
			break;


		case 4:
                        if( strcmp(argv[1], "--move-tree") == 0   ){
				adminOps->move_subtree( argv[2], argv[3]);
				break;
			}	

            
        	case 5:
        
			if( strcmp(argv[1], "--add-partition") == 0   ){
				adminOps->add_partition( argv[2],argv[3],argv[4]);
				break;
			}
		
		default:
			print_illegal_parameters();
			break;


	
	}

	delete adminOps;
}

