/* 
 * File:   raid_data.cpp
 * Author: markus
 *
 * Created on 5. Juni 2012, 20:16
 */

#ifndef RAID_DATA_CPP
#define	RAID_DATA_CPP

#include "components/raidlibs/raid_data.h"
#include "components/OperationManager/OpData.h"



void free_StripeUnit(struct StripeUnit *su)
{
    if (su->newdata!=NULL)      free(su->newdata);
    if (su->olddata!=NULL)      free(su->olddata);
    if (su->paritydata!=NULL)   free(su->paritydata);
    if (su->do_recv!=NULL)
    {
        free(su->do_recv->data);
        delete su->do_recv;
    }
    delete su;
}

void free_StripeLayout(struct Stripe_layout *sl)
{
    std::map<StripeUnitId,struct StripeUnit*>::iterator it = sl->blockmap->begin();
    while (it!=sl->blockmap->end())
    {
        if (it->second!=NULL) free_StripeUnit(it->second);
        sl->blockmap->erase(it++);
    }
    delete sl->blockmap;
    delete sl;
}

void free_FileStripeList(struct FileStripeList *fsl)
{
    std::map<StripeId,struct Stripe_layout*>::iterator it = fsl->stripemap->begin();
    while (it!=fsl->stripemap->end())
    {
        if (it->second!=NULL)free_StripeLayout(it->second);
        fsl->stripemap->erase(it++);
    }
    delete fsl->stripemap;
    delete fsl;
}

#endif	/* RAID_DATA_CPP */

