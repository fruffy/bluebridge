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
#include <arpa/inet.h>

#include "../client_lib.h"
extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off_t __offset);
extern ssize_t pwrite (int __fd, const void *__buf, size_t __nbytes, __off_t __offset);


struct mem {
    struct u_int8 **memList;
    int block_size;
    int nblocks;
};


void fill_mem(struct mem *r) {
    r->memList = malloc(sizeof(struct in6_addr) * r->nblocks);
    for (int i = 0; i<r->nblocks; i++){
        // Generate a random IPv6 address out of a set of available hosts
        r->memList[i] = malloc(r->block_size);
    }
}

struct mem *mem_allocate(int nblocks) {
    struct mem *r;
    r = malloc(sizeof(*r));
    if(!r) return 0;
    r->block_size = BLOCK_SIZE;
    r->nblocks = nblocks;
    fill_mem(r);
    return r;
}

void mem_write(struct mem *r, int block, char *data) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"disk_write: invalid block #%d\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    memcpy(r->memList[block], data, r->block_size);
}

void mem_read(struct mem *r, int block, char *data) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%d\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    memcpy(data, r->memList[block], r->block_size);

}

int mem_blocks(struct mem *r) {
    return r->nblocks;
}

void mem_deallocate(struct mem *r) {
    for (int i = 0; i<r->nblocks; i++){
        free(r->memList[i]);
    }
    close_sockets();
    free(r);
}
