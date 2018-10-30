#include "gtest/gtest.h"

#include <string>
#include <sstream>
#include <fstream>

#include "components/raidlibs/Libraid5.h"
#include "components/raidlibs/raid_data.h"


namespace
{

class Libraid5Test : public ::testing::Test
{
public:
    Libraid5 *p_raid;
    Logger *log;
    ServerManager *p_sm;
    uint8_t gsize;
    size_t susize;
    uint16_t servercnt;
    InodeNumber inum;
    
    Libraid5Test()
    {
        log = new Logger();
        string s = string("/tmp/Libraid5Test.log");
	log->set_log_location(s);
        log->set_console_output(false);
        p_sm = new ServerManager(log, 11000);
        p_raid = new Libraid5(log);
        p_raid->register_server_manager(p_sm);
        susize = 10000;
        gsize = 8;
        servercnt = 16;
        inum=3;
    }
    ~Libraid5Test()
    {
    }

protected:
    void SetUp()
    {
        p_sm->register_server(0,"192.168.0.10");
        p_sm->register_server(1,"192.168.0.11");
        p_sm->register_server(2,"192.168.0.12");
        p_sm->register_server(3,"192.168.0.13");
        p_sm->register_server(4,"192.168.0.14");
        p_sm->register_server(5,"192.168.0.15");
        p_sm->register_server(6,"192.168.0.16");
        p_sm->register_server(7,"192.168.0.17");
        p_sm->register_server(8,"192.168.0.18");
        p_sm->register_server(9,"192.168.0.19");
        p_sm->register_server(10,"192.168.0.20");
        p_sm->register_server(11,"192.168.0.21");
        p_sm->register_server(12,"192.168.0.22");
        p_sm->register_server(13,"192.168.0.23");
        p_sm->register_server(14,"192.168.0.24");
        p_sm->register_server(15,"192.168.0.25");
        
        p_raid->setGroupsize(gsize);
        p_raid->setStripeUnitSize(susize);
        p_raid->setServercnt(servercnt);
    }

    void TearDown()
    {
    }
};


TEST_F(Libraid5Test, create_init_fl)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    struct filelayout_raid5 *p_r5 = (struct filelayout_raid5 *) &fl;
    
    ASSERT_EQ(p_r5->type,raid5);
    ASSERT_EQ(p_r5->groupsize,this->gsize);
    ASSERT_EQ(p_r5->stripeunitsize,this->susize);
    ASSERT_EQ(p_r5->servercount,this->servercnt);
    ASSERT_EQ(p_r5->serverids[0],0);
    ASSERT_EQ(p_r5->serverids[1],1);
    ASSERT_EQ(p_r5->serverids[2],2);
    ASSERT_EQ(p_r5->serverids[3],3);
    ASSERT_EQ(p_r5->serverids[4],4);
    ASSERT_EQ(p_r5->serverids[5],5);
    ASSERT_EQ(p_r5->serverids[6],6);
    ASSERT_EQ(p_r5->serverids[7],7);
    ASSERT_EQ(p_r5->serverids[8],8);
    ASSERT_EQ(p_r5->serverids[9],9);
    ASSERT_EQ(p_r5->serverids[10],10);
    ASSERT_EQ(p_r5->serverids[11],11);
    ASSERT_EQ(p_r5->serverids[12],12);
    ASSERT_EQ(p_r5->serverids[13],13);
    ASSERT_EQ(p_r5->serverids[14],14);
    ASSERT_EQ(p_r5->serverids[15],15);
}

TEST_F(Libraid5Test, stripe_layout)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    struct filelayout_raid5 *p_r5 = (struct filelayout_raid5 *) &fl;    
    ASSERT_EQ(p_r5->type,raid5);
    ASSERT_EQ(p_r5->groupsize,this->gsize);
    ASSERT_EQ(p_r5->stripeunitsize,this->susize);
    ASSERT_EQ(p_r5->servercount,this->servercnt);
    ASSERT_EQ(p_r5->serverids[0],0);
    ASSERT_EQ(p_r5->serverids[1],1);
    ASSERT_EQ(p_r5->serverids[2],2);
    ASSERT_EQ(p_r5->serverids[3],3);
    ASSERT_EQ(p_r5->serverids[4],4);
    ASSERT_EQ(p_r5->serverids[5],5);
    ASSERT_EQ(p_r5->serverids[6],6);
    ASSERT_EQ(p_r5->serverids[7],7);
    ASSERT_EQ(p_r5->serverids[8],8);
    ASSERT_EQ(p_r5->serverids[9],9);
    ASSERT_EQ(p_r5->serverids[10],10);
    ASSERT_EQ(p_r5->serverids[11],11);
    ASSERT_EQ(p_r5->serverids[12],12);
    ASSERT_EQ(p_r5->serverids[13],13);
    ASSERT_EQ(p_r5->serverids[14],14);
    ASSERT_EQ(p_r5->serverids[15],15);
    struct Stripe_layout *p_sl;
    size_t offset = 0;
    for (serverid_t i=0; i<p_r5->servercount*2; i++)
    {
        log->debug_log("Run for server id:%u. offset:%llu",i,offset);
        p_sl = p_raid->get_stripe(p_r5, offset);
        ASSERT_TRUE(p_sl!=NULL);
        ASSERT_EQ(i%p_r5->servercount,p_sl->parityserver_id);
        offset+=get_stripe_size(p_r5);
    }    
}


TEST_F(Libraid5Test, iscoordinator)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    struct filelayout_raid5 *p_r5 = (struct filelayout_raid5 *) &fl;    

    size_t offset = 0;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 2);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    
    
    offset = 19999;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 2);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 0);
    
    offset = 20000;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 2);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 8), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 9), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 10), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 11), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 12), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 13), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 14), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 15), 0);    
    
    
    offset = 560000;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 8), 2);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 9), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 10), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 11), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 12), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 13), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 14), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 15), 0);
    
    offset = 629999;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 8), 2);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 9), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 10), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 11), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 12), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 13), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 14), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 15), 1);
        
    offset = 630000;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 8), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 9), 2);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,10), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,11), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,12), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,13), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,14), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,15), 0);
}


TEST_F(Libraid5Test, get_server_id_1)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    struct filelayout_raid5 *p_r5 = (struct filelayout_raid5 *) &fl;    

    ASSERT_EQ(p_raid->get_server_id(p_r5, 0, 0), 1);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 0, 1), 2);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 0, 2), 3);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 0, 3), 4);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 0, 4), 5);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 0, 5), 6);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 0, 6), 7);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 1, 7), 0);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 1, 8), 2);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 1, 9), 3);
    
    ASSERT_EQ(p_raid->get_server_id(p_r5, 8, 56), 9);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 8, 57), 10);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 8, 58), 11);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 8, 59), 12);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 8, 60), 13);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 8, 61), 14);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 8, 62), 15);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 9, 63), 8);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 9, 64), 10);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 9, 65), 11);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 9, 66), 12);
    ASSERT_EQ(p_raid->get_server_id(p_r5, 9, 67), 13);    
}


TEST_F(Libraid5Test, get_primary_coordinator_1)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    struct filelayout_raid5 *p_r5 = (struct filelayout_raid5 *) &fl;
    
    ASSERT_EQ(p_raid->get_primary_coord(p_r5,      0), 0); // 1st group starts
    ASSERT_EQ(p_raid->get_primary_coord(p_r5,  69999), 0);
    ASSERT_EQ(p_raid->get_primary_coord(p_r5,  70000), 1); // next server
    ASSERT_EQ(p_raid->get_primary_coord(p_r5, 139999), 1);
    ASSERT_EQ(p_raid->get_primary_coord(p_r5, 140000), 2);
    ASSERT_EQ(p_raid->get_primary_coord(p_r5, 559999), 7); // 1st group ends
    ASSERT_EQ(p_raid->get_primary_coord(p_r5, 560000), 8); // 2nd group starts    
    ASSERT_EQ(p_raid->get_primary_coord(p_r5,1119999), 15); //2nd group ends
    ASSERT_EQ(p_raid->get_primary_coord(p_r5,1120000),  0); //1nd group starts
}

TEST_F(Libraid5Test, get_secondary_coordinator_1)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    struct filelayout_raid5 *p_r5 = (struct filelayout_raid5 *) &fl;
    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,      0), 1); // 1st group starts
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,   9999), 1); // 
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,  10000), 2); 
    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,  69999), 7);
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,  70000), 0); // next server    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 139999), 7);
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 140000), 0);
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 559999), 6); // 1st group ends
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 560000), 9); // 2nd group starts    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,1119999), 14); //2nd group ends
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,1120000), 1); //1nd group starts
}

}//namespace