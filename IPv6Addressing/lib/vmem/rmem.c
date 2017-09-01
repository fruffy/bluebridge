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
#include "../client_lib.h"
extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off_t __offset);
extern ssize_t pwrite (int __fd, const void *__buf, size_t __nbytes, __off_t __offset);

#define TARGETPORT "5000"

struct rmem {
	int sockfd;
	struct sockaddr_in6 *targetIP;
	struct in6_addr *memList;
	int block_size;
	int nblocks;
};



void rmem_init_sockets(struct rmem *r) {
    //p = bindSocket(p, servinfo, &sockfd);
    struct config myConf = configure_bluebridge("tmp/config/distMem.cnf", 0);
    r->targetIP = init_rcv_socket(&myConf);
    init_send_socket(&myConf);
    r->sockfd = get_rcv_socket();
}

void fill_rmem(struct rmem *r) {
	struct in6_addr *memList = malloc(sizeof(struct in6_addr) * r->nblocks);
	for (int i = 0; i<r->nblocks; i++){
		memList[i] = allocateRemoteMem(r->targetIP);
	}
	r->memList = memList;
}

struct rmem *rmem_allocate(int nblocks)
{
	struct rmem *r;
	r = malloc(sizeof(*r));
	if(!r) return 0;

	rmem_init_sockets(r);
	if(r->sockfd<0) {
		free(r);
		return 0;
	}
	r->block_size = BLOCK_SIZE;
	r->nblocks = nblocks;
	fill_rmem(r);
	return r;
}

void rmem_write(struct rmem *r, int block, char *data )
{
	if(block<0 || block>=r->nblocks) {
		fprintf(stderr,"disk_write: invalid block #%d\n",block);
		abort();
	}
	// Get pointer to page data in (simulated) physical memory
	writeRemoteMem(r->targetIP, data, &r->memList[block]);
}

void rmem_read( struct rmem *r, int block, char *data )
{
	if(block<0 || block>=r->nblocks) {
		fprintf(stderr,"disk_read: invalid block #%d\n",block);
		abort();
	}
	// Get pointer to page data in (simulated) physical memory
	memcpy(data, getRemoteMem(r->targetIP, &r->memList[block]), r->block_size);

}

int rmem_blocks( struct rmem *r )
{
	return r->nblocks;
}

void rmem_deallocate( struct rmem *r )
{
	close(r->sockfd);
	free(r);
}
