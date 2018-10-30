/**
 * @file mlt_file_provider.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.05.2011
 */

#ifndef MLT_FILE_PROVIDER_H_
#define MLT_FILE_PROVIDER_H_

#include "mlt.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C"
{
#endif

int write_mlt(struct mlt* list, const char* file_path);
int write_entry(struct mlt_entry* e, FILE* fp);

struct mlt* read_mlt(const char* file_path);

struct mlt_entry* traverse_pre_order(struct mlt_entry* e, FILE* fp);


// Internal
int write_int(FILE* fp, int i);
int read_int();

int write_server(FILE* fp, struct asyn_tcp_server* s);
struct asyn_tcp_server* read_server(FILE* fp, struct mlt* list);


#ifdef __cplusplus
}
#endif

#endif /* MLT_FILE_PROVIDER_H_ */
