/* 
 * File:   Libraid4.h
 * Author: markus
 *
 * Created on 18. Mai 2012, 22:03
 */

#ifndef LIBRAID4_H
#define	LIBRAID4_H

#include "global_types.h"
#include "components/raidlibs/raid_data.h"
#include "components/network/ServerManager.h"
#include "logging/Logger.h"

#include "math.h"

#define OPERATION_THRESHOLD_PERCENT 30 


class Libraid4 {
public:
    Libraid4(Logger *p_log);
    Libraid4(const Libraid4& orig);
    virtual ~Libraid4();   

    void setGroupsize(uint8_t size);
    void setStripeUnitSize(size_t size);
    void setServercnt(uint16_t cnt);
    void register_server_manager(ServerManager *p_sman);
    int create_initial_filelayout(InodeNumber inum, filelayout *p_fl);
    
    struct Stripe_layout* get_stripe(filelayout_raid *fl, size_t offset);
    struct Stripe_layout* get_stripe(filelayout_raid *fl, size_t offset, size_t length);
    struct FileStripeList* get_stripes(filelayout_raid *fl, size_t offset, size_t length);
    void printFileStripeList(struct FileStripeList *p);
    void printFL(filelayout_raid *fl);
    void printStripeLayout(struct Stripe_layout *p);
    
    //serverid_t          get_parity_id(filelayout_raid *fl, StripeId id);
    uint8_t             is_coordinator( filelayout_raid *fl, size_t offset, serverid_t myid);
    //serverid_t          get_primary_coord( filelayout_raid *fl, size_t offset);
    serverid_t          get_secondary_coord( filelayout_raid *fl, size_t offset);
    StripeUnitId        get_my_stripeunitid( filelayout_raid *fl, size_t offset, serverid_t myid);
    StripeId            getStripeId( filelayout_raid *fl, size_t offset);

private:
    ServerManager *p_sman;
    Logger *log;
    uint8_t groupsize;
    size_t susize;
    uint16_t servercnt;
    
    struct Stripe_layout* getNewStripeLayout();
    struct FileStripeList* getNewFileStripeList();
};

inline   serverid_t get_server_id ( filelayout_raid *fl, StripeId sid, StripeUnitId id);
static inline   serverid_t get_server_id_gid( filelayout_raid *fl, uint16_t groupid, StripeUnitId suid);
//inline serverid_t get_coordinator(filelayout_raid *fl, size_t offset);

static inline uint16_t get_group_parityid( filelayout_raid *fl, uint16_t gid);
static inline uint16_t get_groupid( filelayout_raid *fl, StripeId sid);
static inline uint16_t get_groupcount( filelayout_raid *fl);
inline size_t get_stripe_size( filelayout_raid *fl);
static inline StripeId get_stripe_id( filelayout_raid *fl, size_t offset);
static size_t get_stripe_start( filelayout_raid *fl, size_t offset);
static size_t get_stripe_end( filelayout_raid *fl, size_t offset);
inline serverid_t _get_parity_id(filelayout_raid *fl, StripeId id);
static StripeUnitId get_stripeunit_id( filelayout_raid *fl, size_t offset);


inline serverid_t get_coordinator(filelayout_raid *fl, size_t offset)
{
    return get_group_parityid(fl, get_groupid(fl,get_stripe_id(fl,offset)));
}

static inline uint16_t get_group_parityid( filelayout_raid *fl, uint16_t gid)
{
   return (fl->raid4.groupsize*gid)+fl->raid4.groupsize-1;
}

static inline uint16_t get_groupid( filelayout_raid *fl, StripeId sid)
{
   return (sid/fl->raid4.groupsize)%(fl->raid4.servercount/fl->raid4.groupsize);
}

static inline uint16_t get_groupcount( filelayout_raid *fl)
{
    return fl->raid4.servercount/fl->raid4.groupsize;
}

inline size_t get_stripe_size( filelayout_raid *fl)
{
    return (fl->raid4.groupsize-1)*fl->raid4.stripeunitsize;
}

static inline StripeId get_stripe_id( filelayout_raid *fl, size_t offset)
{
    return (offset/get_stripe_size(fl));
}


#endif	/* LIBRAID4_H */

