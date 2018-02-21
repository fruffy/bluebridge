/*
 * busexmp - example memory-based block device using BUSE
 * Copyright (C) 2013 Adam Cozzette
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "../lib/client_lib.h"
#include "../lib/utils.h"

#include "../lib/buse/buse.h"

static int xmpl_debug = 1;

struct rmem {
    struct in6_memaddr *memList;
    int block_size;
    uint64_t nblocks;
    struct sockaddr_in6 *targetIP;
} r;

int PAGE_SIZE = 4096;

static int xmp_read(void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
/*    if (*(int *)userdata)
        fprintf(stderr, "R - %lu, %u\n", offset, len);*/
    u_int64_t page_offset = offset/PAGE_SIZE;
    if (len > PAGE_SIZE) {
        for (int i = 0; i< len/PAGE_SIZE; i++) {
            get_rmem((char *) buf + (PAGE_SIZE *i), PAGE_SIZE, r.targetIP, &r.memList[page_offset + i]);
        }
    } else {
        get_rmem(buf, len, r.targetIP, &r.memList[page_offset]);
    }
    //memcpy(buf, (char *)data + offset, len);
/*    char dst_ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6,&r.memList[page_offset], dst_ip, sizeof dst_ip);
    printf("Reading from offset %lu\n", page_offset );
    printf("IP Address: %s\n", dst_ip);*/
    return 0;
}

static int xmp_write(const void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
/*    if (*(int *)userdata)
      fprintf(stderr, "W - %lu, %u\n", offset, len);*/
    u_int64_t page_offset = offset/PAGE_SIZE;
    if (len > PAGE_SIZE) {
        for (int i = 0; i< len/PAGE_SIZE; i++) {
            write_rmem(r.targetIP, (char *) buf + (PAGE_SIZE *i), &r.memList[page_offset + i]);

        }
    } else {
        write_rmem(r.targetIP, (char *)buf, &r.memList[page_offset]);
    }    //memcpy((char *)data + offset, buf, len);
    return 0;
}

static void xmp_disc(void *userdata)
{
    (void)(userdata);
    fprintf(stderr, "Received a disconnect request.\n");
}

static int xmp_flush(void *userdata){
    (void)(userdata);
    fprintf(stderr, "Received a flush request.\n");
    return 0;
}

static int xmp_trim(u_int64_t from, u_int32_t len, void *userdata){
    (void)(userdata);
    fprintf(stderr, "T - %lu, %u\n", from, len);
    return 0;
}


static struct buse_operations aop = {
      .read = xmp_read,
      .write = xmp_write,
      .disc = xmp_disc,
      .flush = xmp_flush,
      .trim = xmp_trim,
      .size_blocks =  (u_int64_t) 42 * 1024 * 1024 * 1024 / 4096,
      //.size_blocks =  (u_int64_t) 23 * 1024 * 1024 * 1024 / 4096,
      .blksize = 4096
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, 
            "Usage:\n"
            "  %s /dev/nbd0\n"
            "Don't forget to load nbd kernel module (`modprobe nbd`) and\n"
            "run example from root.\n", argv[0]);
        return 1;
    }
    r.block_size = PAGE_SIZE;
    struct config myConf = set_bb_config("distmem_client.cnf", 0);
    r.targetIP = init_sockets(&myConf, 0);
    set_host_list(myConf.hosts, myConf.num_hosts);
    r.nblocks = aop.size_blocks;
    printf("Allocating %lu MB of memory, %lu blocks\n", (aop.size_blocks * aop.blksize / (1024*1024)), r.nblocks);
    r.memList = malloc(sizeof(struct in6_memaddr) * r.nblocks);
    uint64_t split = r.nblocks/myConf.num_hosts;
    uint64_t length;
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t offset = split * i;
        if (i == myConf.num_hosts-1)
            length = r.nblocks - offset;
        else 
            length = split;
        struct in6_addr *ipv6Pointer = gen_ip6_target(i);
        memcpy(&(r.targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        struct in6_memaddr *temp = allocate_rmem_bulk(r.targetIP, length);
        memcpy(&r.memList[offset],temp,length *sizeof(struct in6_memaddr) );
        free(temp);
    }

    return buse_main(argv[1], &aop, (void *)&xmpl_debug);
}
