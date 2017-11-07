
/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#ifndef RMEM_H
#define RMEM_H

#define BLOCK_SIZE 4096

struct rmem {
    struct sockaddr_in6 *targetIP;
    struct in6_memaddr *memList;
    int block_size;
    int nblocks;
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

void rmem_write( struct rmem *r, int block, const char *data );

/*
Read exactly BLOCK_SIZE bytes from a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to where the data will be placed.
*/

void rmem_read( struct rmem *r, int block, char *data );

/*
Return the number of blocks in the virtual disk.
*/

int rmem_blocks( struct rmem *r );

/*
Close the virtual disk.
*/

void rmem_deallocate( struct rmem *r );

#endif
