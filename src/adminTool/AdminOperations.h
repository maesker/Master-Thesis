
class AdminOperations
{
	public:
		int add_ds( char * address );
		int delete_ds( char * address );
		int check_ds( char * address );
                int add_mds( char * address );
		int remove_mds( char * address );
		int synch_mlt();
		void list_mds();
		int delete_mds( char * address );
		int check_mds( char * address );
                int init_mds( char * address );
        	int add_partition(char *asyn_tcp_server, char *path, char *inodenum);
		int move_subtree(char *address, char *path);
		void list_partitions();
};
