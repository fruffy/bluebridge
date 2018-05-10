
/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#ifndef RMEM_H
#define RMEM_H

#define BLOCK_SIZE 4096
#include <stdint.h>
#include "../types.h"
struct rmem {
    ip6_memaddr *memList;
    int block_size;
    uint64_t nblocks;
};

void configure_rmem(char *filename);

/*
Create a new virtual disk in the file "filename", with the given number of blocks.
Returns a pointer to a new disk object, or null on failure.
*/
struct rmem *rmem_allocate(int blocks );

/*
Write exactly BLOCK_SIZE bytes to a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to the data to write.
*/

void rmem_write( struct rmem *r, uint64_t block, char *data );

/*
Read exactly BLOCK_SIZE bytes from a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to where the data will be placed.
*/

void rmem_read( struct rmem *r, uint64_t block, char *data );

/*
Return the number of blocks in the virtual disk.
*/

uint64_t rmem_blocks( struct rmem *r );

/*
Close the virtual disk.
*/

void rmem_deallocate( struct rmem *r );
void rmem_close_thread_sockets();
void rmem_init_thread_sockets(int t_id);
#endif
