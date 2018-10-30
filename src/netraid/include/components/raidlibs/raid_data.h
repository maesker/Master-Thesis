/* 
 * File:   raid_data.h
 * Author: markus
 *
 * Created on 15. Februar 2012, 11:51
 */

#ifndef RAID_DATA_H
#define	RAID_DATA_H

#include <map>
#include "global_types.h"

typedef uint32_t StripeId;
typedef uint32_t  StripeUnitId;

//static uint32_t default_stripe_unit_size  = 10;
static const uint16_t default_groupsize = 8;
//static uint16_t default_server_count = 3;
//static size_t   default_stripe_size = (default_groupsize-1)*default_stripe_unit_size;


struct StripeUnit{
    size_t start;
    size_t end;
    size_t opsize;
    serverid_t assignedto;
    char *olddata;
    char *newdata;
    char *paritydata;
    struct data_object *do_recv;
};

enum filelayout_type {
    raid5,
    raid4,
};

struct filelayout_raid5 {
    filelayout_type     type;
    uint8_t             groupsize;
    uint8_t             servercount;
    uint32_t            stripeunitsize;    
    serverid_t          serverids[62];    
};

struct filelayout_raid4 {
    filelayout_type     type;
    uint8_t             groupsize;
    uint8_t             servercount;
    uint32_t            stripeunitsize;    
    serverid_t          serverids[62];    
};

union filelayout_raid {
    struct filelayout_raid5 raid5;
    struct filelayout_raid4 raid4;
};

struct Stripe_layout
{
    std::map<StripeUnitId,struct StripeUnit*> *blockmap;
    serverid_t parityserver_id;
    bool isfull;
};

struct FileStripeList {
    std::map<StripeId,struct Stripe_layout*> *stripemap;
    size_t size;
};

void free_FileStripeList(struct FileStripeList *fsl);
void free_StripeUnit(struct StripeUnit *su);
void free_StripeLayout(struct Stripe_layout *sl);
#endif	/* RAID_DATA_H */

