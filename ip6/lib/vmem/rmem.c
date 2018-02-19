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



static __thread struct sockaddr_in6 *targetIP;

struct config myConf;
void configure_rmem(char *filename) {
    myConf = set_bb_config(filename, 0);
    set_host_list(myConf.hosts, myConf.num_hosts);
}

void rmem_init_sockets() {
    targetIP = init_sockets(&myConf);
}

void rmem_init_thread_sockets(int t_id) {
    targetIP = init_net_thread(t_id, &myConf, 0);
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
    uint64_t length;
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t offset = split * i;
        if (i == myConf.num_hosts-1)
            length = r->nblocks - offset;
        else 
            length = split;
        struct in6_addr *ipv6Pointer = gen_ip6_target(i);
        memcpy(&(targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        struct in6_memaddr *temp = allocate_rmem_bulk(targetIP, length);
        memcpy(&memList[offset],temp,length *sizeof(struct in6_memaddr) );
        free(temp);
    }
    r->memList = memList;
}

struct rmem *rmem_allocate(int nblocks) {
    struct rmem *r;
    r = malloc(sizeof(*r));
    if(!r) return 0;

    rmem_init_sockets();
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
    write_rmem(targetIP, data, &r->memList[block]);
}

void rmem_read( struct rmem *r, uint64_t block, char *data ) {
    if(block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%lu\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory

     get_rmem(data,r->block_size, targetIP, &r->memList[block]);
}

uint64_t rmem_blocks(struct rmem *r) {
    return r->nblocks;
}

void rmem_deallocate(struct rmem *r) {
/*    for (int i = 0; i<r->nblocks; i++){
        free_rmem(r->targetIP, &r->memList[i]);
    }*/
    //free_rmem(r->targetIP, &r->memList[0]);
    //targetIP = init_net_thread(0, &myConf, 0);
    uint64_t split = r->nblocks/myConf.num_hosts;
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t offset = split * i;
        free_rmem(targetIP, &r->memList[offset]);
    }
    //close_sockets();
    free(r);
}
void rmem_close_thread_sockets() {
    close_sockets();
}
