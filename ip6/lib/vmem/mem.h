
/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#ifndef mem_H
#define mem_H

#define BLOCK_SIZE 4096

/*
Create a new virtual disk in the file "filename", with the given number of blocks.
Returns a pointer to a new disk object, or null on failure.
*/
struct mem *mem_allocate(int blocks );

/*
Write exactly BLOCK_SIZE bytes to a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to the data to write.
*/

void mem_write( struct mem *r, int block, const char *data );

/*
Read exactly BLOCK_SIZE bytes from a given block on the virtual disk.
"d" must be a pointer to a virtual disk, "block" is the block number,
and "data" is a pointer to where the data will be placed.
*/

void mem_read( struct mem *r, int block, char *data );

/*
Return the number of blocks in the virtual disk.
*/

int mem_blocks( struct mem *r );

/*
Close the virtual disk.
*/

void mem_deallocate( struct mem *r );

#endif
