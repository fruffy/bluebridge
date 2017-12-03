/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "../client_lib.h"
#include "../utils.h"
#include "rmem.h"

extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off_t __offset);
extern ssize_t pwrite (int __fd, const void *__buf, size_t __nbytes, __off_t __offset);



struct config myConf;
void configure_rmem(char *filename) {
    myConf = set_bb_config(filename, 0);
    set_host_list(myConf.hosts, myConf.num_hosts);
}

void rmem_init_sockets(struct rmem *r) {
    r->targetIP = init_sockets(&myConf);
    r->targetIP->sin6_port = htons(strtol(myConf.server_port, (char **)NULL, 10));
}

void fill_rmem(struct rmem *r) {
/*    struct in6_memaddr *memList = malloc(sizeof(struct in6_addr) * r->nblocks);
    for (int i = 0; i<r->nblocks; i++){
        // Generate a random IPv6 address out of a set of available hosts
        struct in6_addr *ipv6Pointer = gen_rdm_IPv6Target();
        memcpy(&(r->targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        memList[i] = allocate_rmem(r->targetIP);
    }*/
    struct in6_memaddr *memList = malloc(sizeof(struct in6_memaddr) * r->nblocks);
    uint64_t split = r->nblocks/myConf.num_hosts;
    int length;
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t offset = split * i;
        if (i == myConf.num_hosts-1)
            length = r->nblocks - offset;
        else 
            length = split;
        struct in6_addr *ipv6Pointer = gen_IPv6Target(i);
        memcpy(&(r->targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        struct in6_memaddr *temp = allocate_rmem_bulk(r->targetIP, length);
        memcpy(&memList[offset],temp,length *sizeof(struct in6_memaddr) );
        free(temp);
    }
    r->memList = memList;
}

struct rmem *rmem_allocate(int nblocks) {
    struct rmem *r;
    r = malloc(sizeof(*r));
    if(!r) return 0;

    rmem_init_sockets(r);
    r->block_size = BLOCK_SIZE;
    r->nblocks = nblocks;
    fill_rmem(r);
    return r;
}

void rmem_write(struct rmem *r, uint64_t block, char *data ) {
    if(block>=r->nblocks) {
        fprintf(stderr,"disk_write: invalid block #%lu\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    write_rmem(r->targetIP, data, &r->memList[block]);
}

void rmem_read( struct rmem *r, uint64_t block, char *data ) {
    if(block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%lu\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    memcpy(data, get_rmem(r->targetIP, &r->memList[block]), r->block_size);

}

uint64_t rmem_blocks(struct rmem *r) {
    return r->nblocks;
}

void rmem_deallocate(struct rmem *r) {
/*    for (int i = 0; i<r->nblocks; i++){
        free_rmem(r->targetIP, &r->memList[i]);
    }*/
    free_rmem(r->targetIP, &r->memList[0]);
    close_sockets();
    free(r);
}
