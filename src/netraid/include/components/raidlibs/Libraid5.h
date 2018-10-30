/* 
 * File:   libraid5.h
 * Author: markus
 *
 * Created on 13. Januar 2012, 23:34
 */

#ifndef LIBRAID5_H
#define	LIBRAID5_H

#include "global_types.h"
#include "components/raidlibs/raid_data.h"
#include "components/network/ServerManager.h"
#include "logging/Logger.h"

#include "math.h"

#define OPERATION_THRESHOLD_PERCENT 30 


class Libraid5 {
public:
    Libraid5(Logger *p_log);
    Libraid5(const Libraid5& orig);
    virtual ~Libraid5();   

    void setGroupsize(uint8_t size);
    void setStripeUnitSize(size_t size);
    void setServercnt(uint16_t cnt);
    void register_server_manager(ServerManager *p_sman);
    int create_initial_filelayout(InodeNumber inum, filelayout *p_fl);
    
    struct Stripe_layout* get_stripe(filelayout_raid5 *fl, size_t offset);
    struct Stripe_layout* get_stripe(filelayout_raid5 *fl, size_t offset, size_t length);
    struct FileStripeList* get_stripes(filelayout_raid5 *fl, size_t offset, size_t length);
    void printFileStripeList(struct FileStripeList *p);
    void printFL(filelayout_raid5 *fl);
    void printStripeLayout(struct Stripe_layout *p);
    
    serverid_t          get_parity_id(filelayout_raid5 *fl, StripeId id);
    serverid_t          get_server_id (struct filelayout_raid5 *fl, StripeId sid, StripeUnitId id);
    uint8_t             is_coordinator(struct filelayout_raid5 *fl, size_t offset, serverid_t myid);
    serverid_t          get_primary_coord(struct filelayout_raid5 *fl, size_t offset);
    serverid_t          get_secondary_coord(struct filelayout_raid5 *fl, size_t offset);
    StripeUnitId        get_my_stripeunitid(struct filelayout_raid5 *fl, size_t offset, serverid_t myid);
    StripeId            getStripeId(struct filelayout_raid5 *fl, size_t offset);
    
private:
    ServerManager *p_sman;
    Logger *log;
    uint8_t groupsize;
    size_t susize;
    uint16_t servercnt;
    
    struct Stripe_layout* getNewStripeLayout();
    struct FileStripeList* getNewFileStripeList();
};


static inline uint32_t get_groupid(struct filelayout_raid5 *fl, StripeId sid);
static inline uint32_t get_groupcount(struct filelayout_raid5 *fl);
inline size_t get_stripe_size(struct filelayout_raid5 *fl);
static inline StripeId get_stripe_id(struct filelayout_raid5 *fl, size_t offset);
static size_t get_stripe_start(struct filelayout_raid5 *fl, size_t offset);
static size_t get_stripe_end(struct filelayout_raid5 *fl, size_t offset);
static inline serverid_t _get_parity_id(filelayout_raid5 *fl, StripeId id);
static StripeUnitId get_stripeunit_id(struct filelayout_raid5 *fl, size_t offset);


#endif	/* LIBRAID5_H */
