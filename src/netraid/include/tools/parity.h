/* 
 * File:   parity.h
 * Author: markus
 *
 * Created on 21. Dezember 2011, 16:42
 */

#ifndef PARITY_H
#define	PARITY_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void calc_parity(void *ina, void *inb, void *out, size_t  len);
void calc_parity (void* in, size_t length, void *out);

#endif	/* PARITY_H */

