/** 
 * @file MockupObjects.cpp
 * @author Markus Mäsker, maesker@gmx.net
 * @date 22.08.2011
 * */

#include "mm/mds/MockupObjects.h"

FsalEInodeRequest* get_mock_EInodeRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num)
{
    struct FsalEInodeRequest *p_fsal = new FsalEInodeRequest;
    p_fsal->type=einode_request;
    p_fsal->partition_root_inode_number=partition_root;
    p_fsal->inode_number=inode_num;
    return p_fsal;
}

FsalDeleteInodeRequest* get_mock_FsalDeleteInodeRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num)
{
    struct FsalDeleteInodeRequest *p_fsal = new FsalDeleteInodeRequest;
    p_fsal->type=delete_inode_request;
    p_fsal->partition_root_inode_number=partition_root;
    p_fsal->inode_number=inode_num;
    return p_fsal;
}

FsalLookupInodeNumberByNameRequest* get_mock_FsalLookupInodeNumberByNameRequestStructure(
    InodeNumber partition_root,
    InodeNumber parent_inode_num,
    FsObjectName *p_name)
{
    struct FsalLookupInodeNumberByNameRequest *p_fsal = new FsalLookupInodeNumberByNameRequest;
    p_fsal->type = lookup_inode_number_request;
    p_fsal->partition_root_inode_number = partition_root;
    p_fsal->parent_inode_number = parent_inode_num;
    memcpy(&p_fsal->name, p_name, sizeof(FsObjectName));
    return p_fsal;
}

FsalEInodeRequest* get_mock_FsalEInodeRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num)
{
    struct FsalEInodeRequest *p_fsal = new FsalEInodeRequest;
    p_fsal->type = einode_request;
    p_fsal->partition_root_inode_number = partition_root;
    p_fsal->inode_number = inode_num;
    return p_fsal;
}


FsalReaddirRequest* get_mock_FsalReaddirRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num,
    ReaddirOffset offset )
{
    struct FsalReaddirRequest *p_fsal = new FsalReaddirRequest;
    p_fsal->type = read_dir_request;
    p_fsal->partition_root_inode_number = partition_root;
    p_fsal->inode_number = inode_num;
    p_fsal->offset = offset;
    return p_fsal;
}


FsalUpdateAttributesRequest* get_mock_FSALUpdateAttrRequest(
    InodeNumber partition_root_inode_number,
    InodeNumber inode_number,
    inode_update_attributes_t attrs,
    int attribute_bitfield)
{
    struct FsalUpdateAttributesRequest *p_fsal = new FsalUpdateAttributesRequest;
    p_fsal->type = update_attributes_request;
    p_fsal->partition_root_inode_number = partition_root_inode_number;
    p_fsal->inode_number = inode_number;
    p_fsal->attrs = attrs;
    p_fsal->attribute_bitfield = attribute_bitfield;
}


FsalCreateEInodeRequest* get_mock_DIR_FsalCreateFileInodeRequest()
{
    struct FsalCreateEInodeRequest *ptr = new FsalCreateEInodeRequest;
    inode_create_attributes_t attrs;
    struct stat buffer;
    // write the root inode data
    stat("/tmp", &buffer);
    attrs.mode = buffer.st_mode;
    attrs.size = 512;
    attrs.gid = generate_gid();
    attrs.uid = generate_uid();
    attrs.has_acl = 1 ;
    generate_fsobjectname(&attrs.name,0);
    ptr->type = create_file_einode_request;
    ptr->partition_root_inode_number = generate_random_inodenumber();
    ptr->parent_inode_number = generate_random_inodenumber();
    memcpy(&ptr->attrs, &attrs, sizeof(attrs));
    return ptr;
}

FsalCreateEInodeRequest* get_mock_FsalCreateFileInodeRequest()
{
    FsalCreateEInodeRequest *ptr = new FsalCreateEInodeRequest;
    inode_create_attributes_t attrs;
    attrs.mode = 0;
    attrs.size = 0;
    attrs.gid = 0;
    attrs.uid = 0;
    attrs.has_acl =1 ;
    generate_fsobjectname(&attrs.name,0);
    ptr->type = create_file_einode_request;
    ptr->partition_root_inode_number = generate_random_inodenumber();
    ptr->parent_inode_number = generate_random_inodenumber();
    memcpy(&ptr->attrs, &attrs, sizeof(attrs));
    return ptr;
}


FsalInodeNumberHierarchyRequest* get_mock_FsalParentHierarchyRequest()
{
    FsalInodeNumberHierarchyRequest *ptr = new FsalInodeNumberHierarchyRequest;
    ptr->type = parent_inode_hierarchy_request;
    ptr->partition_root_inode_number = generate_random_inodenumber();
    ptr->inode_number = generate_random_inodenumber();
    return ptr;
}





char generate_random_fsname_char()
{
    char arr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int ran = rand()% sizeof(arr);
    char c = arr[ran];
    return c;
}

int generate_fsobjectname(FsObjectName *ptr, int len)
{
    FsObjectName name;
    if (len==0)
    {
        len = (rand()%MAX_NAME_LEN);
    }

    int i=0;
    while (i<=len)
    {
        name[i] = generate_random_fsname_char();
        i++;
    }
    name[len]='\0';
    memmove(ptr,&name,sizeof(FsObjectName));
}

InodeNumber generate_random_inodenumber()
{
    return (InodeNumber) rand();
}

int generate_link_count()
{
    return rand()% 100;
}

int generate_gid()
{
    return rand()% 10000;
}


int generate_uid()
{
    return rand()% 10000;
}

time_t generate_time()
{
    return time(NULL);
}

int generate_access_rights()
{
    return 7;
}

size_t generate_size_tx()
{
    return (size_t) rand();
}

mode_t generate_mode_t()
{
    return (mode_t ) 0200;
}

int get_mock_inode(  InodeNumber *inumber, mds_inode_t *node)
{
    char *layout = (char *)"Testlayout";
    node->ctime = (time_t) generate_time();
    node->atime = (time_t) generate_time();
    node->mtime = (time_t) generate_time();
    node->uid = generate_uid();
    node->gid = generate_gid();
    //node->access_rights = generate_access_rights();
    node->mode = generate_mode_t();
    node->has_acl = 0;
    node->inode_number = generate_random_inodenumber();
    node->size = generate_size_tx();
    node->file_type = file;
    memset(&(node->layout_info[0]),0,LAYOUT_INFO_LEN);
    memcpy(&(node->layout_info[0]), layout ,strlen(layout));
    node->link_count=generate_link_count();
    return 0;
}

int get_mock_readdirreturn(
    InodeNumber *inumber,
    ReaddirOffset *off,
    ReadDirReturn *rdir)
{
    int it = 2;
    for (int i=0; i<it; i++)
    {
        EInode ei;
        get_mock_einode(inumber,&ei);
        memcpy(&rdir->nodes[i], &ei, sizeof(ei));
        rdir->dir_size=it;
    }
    return 0;
}

int get_mock_einode(InodeNumber *inumber, EInode *einode)
{
    FsObjectName name;
    generate_fsobjectname(&name,5);
    memcpy(&einode->name,name,sizeof(FsObjectName));
    mds_inode_t inode;
    get_mock_inode(inumber, &inode);
    memcpy(&einode->inode,&inode, sizeof(inode));
    return 0;
}
int get_mock_FsalInvalidTypeRequest(void *ptr)
{

}

int get_mock_inode_number_lookup(
    InodeNumber *parent_inode_number,
    FsObjectName *name,
    InodeNumber *inumber)
{
    *inumber = generate_random_inodenumber();
    return 0;
}


struct mlt* create_dummy_mlt()
{
    struct asyn_tcp_server *mds_server = (struct asyn_tcp_server*) malloc(sizeof(struct asyn_tcp_server));
    mds_server->address = (char *) malloc(128);
    strcpy(mds_server->address, "127.0.0.1");
    mds_server->port = (unsigned short) 9000;
    
    struct asyn_tcp_server *mds_server2 = (struct asyn_tcp_server*) malloc(sizeof(struct asyn_tcp_server));
    mds_server2->address = (char *) malloc(128);
    strcpy(mds_server2->address,  "192.168.0.1");
    mds_server2->port = (unsigned short) 9001;
    
    struct asyn_tcp_server *mds_server3 = (struct asyn_tcp_server*) malloc(sizeof(struct asyn_tcp_server));
    mds_server3->address = (char *) malloc(128);
    strcpy(mds_server3->address,"192.168.0.2");
    mds_server3->port = (unsigned short) 9002;
    
    InodeNumber root_inode = 1;
    InodeNumber inode2 = 2;
    InodeNumber inode3 = 3;

    struct mlt *p_mlt = (struct mlt*) malloc(sizeof(struct mlt));
    p_mlt = mlt_create_new_mlt( mds_server, 1, root_inode);

    mlt_add_new_entry(p_mlt, mds_server2, 2, inode2, "/foo");
    mlt_add_new_entry(p_mlt, mds_server3, 3, inode3, "/bar");

    //mlt_add_new_entry(struct mlt* list, struct server* mds, int export_id, InodeNumber root_inode, const char* path_string);
    //mlt_add_new_entry(p_mlt, struct server* mds, int export_id, InodeNumber root_inode, const char* path_string);
    return p_mlt;
}

/*
void generate_export_block(struct export_block *p_eb)
{
    p_eb->export_id=1;
    p_eb->path = "/nfs/test1";
    (*p_eb).pseudo = "/preudoentry";
    (*p_eb).root_access = "localhost";
    (*p_eb).access_type = "RW";
    (*p_eb).access = "*";
    (*p_eb).anonymous_root_uid = "-2";
    (*p_eb).nosuid = 0;
    (*p_eb).nosgid = 0;
    (*p_eb).nfs_protocols = "2,3,4";
    (*p_eb).transport_protocols = "TCP";
    (*p_eb).sec_type = "sys";
    (*p_eb).maxoffsetread = 409600;
    (*p_eb).maxoffsetwrite = 409600;
    (*p_eb).maxread = 409600;
    (*p_eb).maxwrite = 409600;
    (*p_eb).prefread = 409600;
    (*p_eb).prefwrite = 409600;
    (*p_eb).prefreaddir = 409600;
    (*p_eb).filesystem_id = 100;
    (*p_eb).privilegedport = 0;
    (*p_eb).cache_data = 1;
    (*p_eb).max_cache_size = 1024*1024;
    (*p_eb).fs_specific = "cos=1";
    (*p_eb).tag = "ganesha";
    p_eb->next=NULL;
    
}
*/
