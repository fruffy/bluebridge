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
#include "../utils.h"

extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off_t __offset);
extern ssize_t pwrite (int __fd, const void *__buf, size_t __nbytes, __off_t __offset);


struct rmem {
    struct sockaddr_in6 *targetIP;
    struct in6_memaddr *memList;
    int block_size;
    int nblocks;
};


struct config myConf;
void configure_rmem(char *filename) {
    myConf = configure_bluebridge(filename, 0);
    set_host_list(myConf.hosts, myConf.num_hosts);
}

void rmem_init_sockets(struct rmem *r) {
    r->targetIP = init_rcv_socket(&myConf);
    r->targetIP->sin6_port = htons(strtol(myConf.server_port, (char **)NULL, 10));
    init_send_socket(&myConf);
}

void fill_rmem(struct rmem *r) {
    struct in6_memaddr *memList = malloc(sizeof(struct in6_addr) * r->nblocks);
    for (int i = 0; i<r->nblocks; i++){
        // Generate a random IPv6 address out of a set of available hosts
        struct in6_addr *ipv6Pointer = gen_rdm_IPv6Target();
        memcpy(&(r->targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        memList[i] = allocateRemoteMem(r->targetIP);
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

void rmem_write(struct rmem *r, int block, char *data ) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"disk_write: invalid block #%d\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    writeRemoteMem(r->targetIP, data, &r->memList[block]);
}

void rmem_read( struct rmem *r, int block, char *data ) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%d\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    memcpy(data, getRemoteMem(r->targetIP, &r->memList[block]), r->block_size);

}

int rmem_blocks(struct rmem *r) {
    return r->nblocks;
}

void rmem_deallocate(struct rmem *r) {
    for (int i = 0; i<r->nblocks; i++){
        freeRemoteMem(r->targetIP, &r->memList[i]);
    }
    close_sockets();
    free(r);
}
