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

static int xmpl_debug = 0;

struct rmem {
    ip6_memaddr *memList;
    int block_size;
    uint64_t nblocks;
    struct sockaddr_in6 *target_ip;
} r;

static int xmp_read(void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
    if (*(int *)userdata) fprintf(stderr, "R - %lu, %u\n", offset, len);
    (void) userdata;
    u_int64_t page_offset = offset/BLOCK_SIZE;
    read_bulk_rmem(r.target_ip, &r.memList[page_offset], len/BLOCK_SIZE, (char *) buf, len);
    return 0;
}

static int xmp_write(const void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
    if (*(int *)userdata) fprintf(stderr, "W - %lu, %u\n", offset, len);
    (void) userdata;
    u_int64_t page_offset = offset/BLOCK_SIZE;
    write_bulk_rmem(r.target_ip, &r.memList[page_offset], len/BLOCK_SIZE, (char *) buf, len);
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
    u_int64_t page_offset = from/BLOCK_SIZE;
    //trim_rmem(r.target_ip, &r.memList[page_offset], len);
    return 0;
}


static struct buse_operations aop = {
      .read = xmp_read,
      .write = xmp_write,
      .disc = xmp_disc,
      .flush = xmp_flush,
      .trim = xmp_trim,
      .size = (u_int64_t) 4 * 1024 * 1024 * 1024
      //.size_blocks =  (u_int64_t) 4 * 1024 * 1024 * 1024 / BLOCK_SIZE,
      //.size_blocks =  (u_int64_t) 23 * 1024 * 1024 * 1024 / BLOCK_SIZE,
      //.blksize = BLOCK_SIZE
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
    r.block_size = BLOCK_SIZE;
    struct config myConf = set_bb_config("distmem_client.cnf", 0);
    //struct config myConf = set_bb_config("tmp/config/distMem.cnf", 0);
    r.target_ip = init_sockets(&myConf, 0);
    set_host_list(myConf.hosts, myConf.num_hosts);
    //r.nblocks = aop.size_blocks;
    r.nblocks = aop.size/BLOCK_SIZE;
    // printf("Allocating %lu MB of memory, %lu blocks\n", (aop.size_blocks * aop.blksize / (1024*1024)), r.nblocks);
    printf("Allocating %lu MB of memory, %lu blocks\n", (aop.size / (1024*1024)), r.nblocks);
    r.memList = malloc(sizeof(ip6_memaddr) * r.nblocks);
    uint64_t split = r.nblocks/myConf.num_hosts;
    uint64_t length;
    for (int i = 0; i < myConf.num_hosts; i++) {
        uint64_t offset = split * i;
        if (i == myConf.num_hosts-1)
            length = r.nblocks - offset;
        else
            length = split;
        struct in6_addr *ipv6Pointer = get_ip6_target(i);
        memcpy(&(r.target_ip->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        ip6_memaddr *temp = allocate_bulk_rmem(r.target_ip, length);
        memcpy(&r.memList[offset],temp,length *sizeof(ip6_memaddr) );
        free(temp);
    }

    return buse_main(argv[1], &aop, (void *)&xmpl_debug);
}
