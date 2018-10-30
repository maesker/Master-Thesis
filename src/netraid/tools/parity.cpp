#include "tools/parity.h"

void calc_parity(void *ina, void *inb, void *out, size_t  len_x)
{   
    uint64_t *p = (uint64_t *) out;
    uint64_t *a = (uint64_t *) ina;
    uint64_t *b = (uint64_t *) inb;
    size_t len = len_x/8;
    for (len; len>0; len--)
    {
        *p = ( *a ^ *b);
        //printf("%llu,%llu,%llu.\n",*a,*b,*p);
        p++;
        a++;
        b++;        
    }
}

void calc_parity (void *in, size_t length, void *out)
{
    int i = length/sizeof(uint32_t);
    
    uint32_t *tmp_in = (uint32_t*)in;
    uint32_t *tmp_out = (uint32_t*)out;
    *tmp_out=0;
    for (int j=0; j<i; j++)
    {
        *tmp_out = *tmp_in^*tmp_out;
        tmp_in++;
    }
}