/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#include "disk.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off_t __offset);
extern ssize_t pwrite (int __fd, const void *__buf, size_t __nbytes, __off_t __offset);


struct disk {
	int fd;
	uint64_t block_size;
	uint64_t nblocks;
};

struct disk *disk_open( const char *diskname, uint64_t nblocks ) {
	struct disk *d;

	d = malloc(sizeof(*d));
	if(!d) return 0;

	d->fd = open(diskname,O_CREAT|O_RDWR,0777);
	if(d->fd<0) {
		free(d);
		return 0;
	}

	d->block_size = BLOCK_SIZE;
	d->nblocks = nblocks;

	if(ftruncate(d->fd,d->nblocks*d->block_size)<0) {
		close(d->fd);
		free(d);
		return 0;
	}

	return d;
}

void disk_write( struct disk *d, uint64_t block, const uint8_t *data ) {
	if(block>=d->nblocks) {
		fprintf(stderr,"disk_write: invalid block #%lu\n",block);
		abort();
	}

	uint64_t actual = pwrite(d->fd,data,d->block_size,block*d->block_size);
	if(actual!=d->block_size) {
		fprintf(stderr,"disk_write: failed to write block #%lu: %s\n",block,strerror(errno));
		abort();
	}
}

void disk_read( struct disk *d, uint64_t block, uint8_t *data ) {
	if(block>=d->nblocks) {
		fprintf(stderr,"disk_read: invalid block #%lu\n",block);
		abort();
	}

	uint64_t actual = pread(d->fd,data,d->block_size,block*d->block_size);
	if(actual!=d->block_size) {
		fprintf(stderr,"disk_read: failed to read block #%lu: %s\n",block,strerror(errno));
		abort();
	}
}

uint64_t disk_nblocks( struct disk *d ) {
	return d->nblocks;
}

void disk_close( struct disk *d ) {
	close(d->fd);
	free(d);
}
