
/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#ifndef DISK_H
#define DISK_H
#include <stdint.h>

#define BLOCK_SIZE 4096

/*
Create a new virtual disk in the file "filename", with the given number of blocks.
Returns a pointer to a new disk object, or null on failure.
*/

struct disk * disk_open( const char *filename, uint64_t blocks );

/*
Write exactly BLOCK_SIZE bytes to a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to the data to write.
*/

void disk_write( struct disk *d, uint64_t block, const char *data );

/*
Read exactly BLOCK_SIZE bytes from a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to where the data will be placed.
*/

void disk_read( struct disk *d, uint64_t block, char *data );

/*
Return the number of blocks in the virtual disk.
*/

uint64_t disk_nblocks( struct disk *d );

/*
Close the virtual disk.
*/

void disk_close( struct disk *d );

#endif
