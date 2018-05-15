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



static __thread struct sockaddr_in6 *target_ip;

struct config myConf;
void configure_rmem(char *filename) {
    myConf = set_bb_config(filename, 0);
    set_host_list(myConf.hosts, myConf.num_hosts);
}

void rmem_init_sockets() {
    target_ip = init_sockets(&myConf, 0);
}

void rmem_init_thread_sockets(int t_id) {
    target_ip = init_net_thread(t_id, &myConf, 0);
}

void fill_rmem(struct rmem *r) {
    ip6_memaddr *memList = malloc(sizeof(ip6_memaddr) * r->nblocks);
    uint64_t split = r->nblocks/myConf.num_hosts;
    uint64_t length;
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t offset = split * i;
        if (i == myConf.num_hosts-1)
            length = r->nblocks - offset;
        else 
            length = split;
        struct in6_addr *ipv6Pointer = get_ip6_target(i);
        memcpy(&(target_ip->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        ip6_memaddr *temp = allocate_bulk_rmem(target_ip, length);
        memcpy(&memList[offset],temp,length *sizeof(ip6_memaddr) );
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
    write_rmem(target_ip, &r->memList[block], data, BLOCK_SIZE);
}

void rmem_read( struct rmem *r, uint64_t block, char *data ) {
    if(block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%lu\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory

     read_rmem(target_ip, &r->memList[block], data, r->block_size);
}

uint64_t rmem_blocks(struct rmem *r) {
    return r->nblocks;
}

void rmem_deallocate(struct rmem *r) {
/*    for (int i = 0; i<r->nblocks; i++){
        free_rmem(r->target_ip, &r->memList[i]);
    }*/
    //free_rmem(r->target_ip, &r->memList[0]);
    //target_ip = init_net_thread(0, &myConf, 0);
    uint64_t split = r->nblocks/myConf.num_hosts;
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t offset = split * i;
        free_rmem(target_ip, &r->memList[offset]);
    }
    //close_sockets();
    free(r);
}
void rmem_close_thread_sockets() {
    close_sockets();
}
