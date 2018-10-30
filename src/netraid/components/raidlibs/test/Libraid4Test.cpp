#include "gtest/gtest.h"

#include <string>
#include <sstream>
#include <fstream>

#include "components/raidlibs/Libraid4.h"
#include "components/raidlibs/raid_data.h"


namespace
{

class Libraid4Test : public ::testing::Test
{
public:
    Libraid4 *p_raid;
    Logger *log;
    ServerManager *p_sm;
    uint8_t gsize;
    size_t susize;
    uint16_t servercnt;
    InodeNumber inum;
    
    Libraid4Test()
    {
        log = new Logger();
        string s = string("/tmp/Libraid4Test.log");
	log->set_log_location(s);
        log->set_console_output(false);
        p_sm = new ServerManager(log, 11000);
        p_raid = new Libraid4(log);
        p_raid->register_server_manager(p_sm);
        susize = 10000;
        gsize = 8;
        servercnt = 16;
        inum=3;
    }
    ~Libraid4Test()
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


TEST_F(Libraid4Test, create_init_fl)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    struct filelayout_raid4 *p_r5 = (struct filelayout_raid4 *) &fl;
    
    ASSERT_EQ(p_r5->type,raid4);
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

TEST_F(Libraid4Test, stripe_layout)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    filelayout_raid *p_r5 = (filelayout_raid *) &fl;    
    ASSERT_EQ(p_r5->raid4.type,raid4);
    ASSERT_EQ(p_r5->raid4.groupsize,this->gsize);
    ASSERT_EQ(p_r5->raid4.stripeunitsize,this->susize);
    ASSERT_EQ(p_r5->raid4.servercount,this->servercnt);
    ASSERT_EQ(p_r5->raid4.serverids[0],0);
    ASSERT_EQ(p_r5->raid4.serverids[1],1);
    ASSERT_EQ(p_r5->raid4.serverids[2],2);
    ASSERT_EQ(p_r5->raid4.serverids[3],3);
    ASSERT_EQ(p_r5->raid4.serverids[4],4);
    ASSERT_EQ(p_r5->raid4.serverids[5],5);
    ASSERT_EQ(p_r5->raid4.serverids[6],6);
    ASSERT_EQ(p_r5->raid4.serverids[7],7);
    ASSERT_EQ(p_r5->raid4.serverids[8],8);
    ASSERT_EQ(p_r5->raid4.serverids[9],9);
    ASSERT_EQ(p_r5->raid4.serverids[10],10);
    ASSERT_EQ(p_r5->raid4.serverids[11],11);
    ASSERT_EQ(p_r5->raid4.serverids[12],12);
    ASSERT_EQ(p_r5->raid4.serverids[13],13);
    ASSERT_EQ(p_r5->raid4.serverids[14],14);
    ASSERT_EQ(p_r5->raid4.serverids[15],15);
    struct Stripe_layout *p_sl;
    size_t offset = 0;
    for (serverid_t i=0; i<p_r5->raid4.servercount*2; i++)
    {
        log->debug_log("Run for server id:%u. offset:%llu",i,offset);
        p_sl = p_raid->get_stripe(p_r5, offset);
        ASSERT_TRUE(p_sl!=NULL);
        ASSERT_EQ(get_coordinator(p_r5,offset),p_sl->parityserver_id);
        offset+=get_stripe_size(p_r5);
    }    
}


TEST_F(Libraid4Test, iscoordinator)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    filelayout_raid *p_r5 = (filelayout_raid *) &fl;    
    
    size_t offset = 0;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 2);

    
    offset = 69999;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 2);
    
    offset = 70000;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 4), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 5), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 6), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 7), 2);
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
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 8), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 9), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 10), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 11), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 12), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 13), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 14), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 15), 2);
    
    offset = 629999;
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 0), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 1), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 2), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 3), 0);
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
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 14), 1);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 15), 2);
        
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
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset, 9), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,10), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,11), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,12), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,13), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,14), 0);
    ASSERT_EQ(p_raid->is_coordinator(p_r5, offset,15), 2);
}


TEST_F(Libraid4Test, get_server_id_1)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    filelayout_raid *p_r5 = (filelayout_raid *) &fl;    

    ASSERT_EQ(get_server_id(p_r5, 0, 0), 0);
    ASSERT_EQ(get_server_id(p_r5, 0, 1), 1);
    ASSERT_EQ(get_server_id(p_r5, 0, 2), 2);
    ASSERT_EQ(get_server_id(p_r5, 0, 3), 3);
    ASSERT_EQ(get_server_id(p_r5, 0, 4), 4);
    ASSERT_EQ(get_server_id(p_r5, 0, 5), 5);
    ASSERT_EQ(get_server_id(p_r5, 0, 6), 6);
    ASSERT_EQ(get_server_id(p_r5, 1, 0), 0);
    ASSERT_EQ(get_server_id(p_r5, 1, 1), 1);
    ASSERT_EQ(get_server_id(p_r5, 1, 2), 2);
    
    ASSERT_EQ(get_server_id(p_r5, 8, 0), 8);
    ASSERT_EQ(get_server_id(p_r5, 8, 1), 9);
    ASSERT_EQ(get_server_id(p_r5, 8, 2), 10);
    ASSERT_EQ(get_server_id(p_r5, 8, 3), 11);
    ASSERT_EQ(get_server_id(p_r5, 8, 4), 12);
    ASSERT_EQ(get_server_id(p_r5, 8, 5), 13);
    ASSERT_EQ(get_server_id(p_r5, 8, 6), 14);
    ASSERT_EQ(get_server_id(p_r5, 9, 0), 8);
    ASSERT_EQ(get_server_id(p_r5, 9, 1), 9);
    ASSERT_EQ(get_server_id(p_r5, 9, 2), 10);
    ASSERT_EQ(get_server_id(p_r5, 9, 3), 11);
    ASSERT_EQ(get_server_id(p_r5, 9, 4), 12);    
}

TEST_F(Libraid4Test, get_primary_coordinator_1)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    filelayout_raid *p_r5 = (filelayout_raid *) &fl;
    
    ASSERT_EQ(get_coordinator(p_r5,      0), 7); // 1st group starts
    ASSERT_EQ(get_coordinator(p_r5,  69999), 7);
    ASSERT_EQ(get_coordinator(p_r5,  70000), 7); // next server
    ASSERT_EQ(get_coordinator(p_r5, 139999), 7);
    ASSERT_EQ(get_coordinator(p_r5, 140000), 7);
    ASSERT_EQ(get_coordinator(p_r5, 559999), 7); // 1st group ends
    ASSERT_EQ(get_coordinator(p_r5, 560000), 15); // 2nd group starts    
    ASSERT_EQ(get_coordinator(p_r5,1119999), 15); //2nd group ends
    ASSERT_EQ(get_coordinator(p_r5,1120000),  7); //1nd group starts
}

TEST_F(Libraid4Test, get_secondary_coordinator_1)
{
    filelayout fl;    
    int rc = p_raid->create_initial_filelayout(inum,&fl);
    ASSERT_EQ(rc ,0);
    filelayout_raid *p_r5 = (filelayout_raid *) &fl;
    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,      0), 0); // 1st group starts
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,   9999), 0); // 
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,  10000), 1); 
    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,  69999), 6);
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,  70000), 0); // next server    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 139999), 6);
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 140000), 0);
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 559999), 6); // 1st group ends
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5, 560000), 8); // 2nd group starts    
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,1119999), 14); //2nd group ends
    ASSERT_EQ(p_raid->get_secondary_coord(p_r5,1120000), 0); //1nd group starts
}

}//namespace
