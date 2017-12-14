
/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#ifndef RRMEM_H
#define RRMEM_H

#define BLOCK_SIZE 4096
#include "../client_lib.h"

void configure_rrmem(char *filename);

/*
Create a new virtual disk in the file "filename", with the given number of blocks.
Returns a pointer to a new disk object, or null on failure.
*/
struct rrmem *rrmem_allocate(int blocks);

/*
Write exactly BLOCK_SIZE bytes to a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to the data to write.
*/

void rrmem_write( struct rrmem *r, int block, char *data );

/*
Read exactly BLOCK_SIZE bytes from a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to where the data will be placed.
*/

void rrmem_read( struct rrmem *r, int block, char *data );

/*
Return the number of blocks in the virtual disk.
*/

int rrmem_blocks( struct rrmem *r );

/*
Close the virtual disk.
*/

void rrmem_deallocate( struct rrmem *r );

void parity45(char *data, int size, int stripes, char *parity);
void repairStripeFromParity45(char (*repair)[BLOCK_SIZE], char (*stripes)[MAX_HOSTS][BLOCK_SIZE], char (*parity)[BLOCK_SIZE], int missing, int numStripes, int size);
int checkParity45(char (*stripes)[MAX_HOSTS][BLOCK_SIZE], int numStripes, char (*parity)[BLOCK_SIZE], int size);

#endif
