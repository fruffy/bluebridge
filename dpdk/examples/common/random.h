#ifndef _RANDOMBUFS_H
#define _RANDOMBUFS_H

#include <stdlib.h>
#include <inttypes.h>

int prand_seed(char *buf, int num);
double prand_next(char *buf);

void prandom_buffer(int seed, uint8_t *buf, int len);

#endif /* _RANDOMBUFS_H */
