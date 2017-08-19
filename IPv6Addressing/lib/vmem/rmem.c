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
#include "../udpcooked.h"
extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off_t __offset);
extern ssize_t pwrite (int __fd, const void *__buf, size_t __nbytes, __off_t __offset);


struct rmem {
	int sockfd;
	struct sockaddr_in6 *targetIP;
	struct in6_addr *memList;
	int block_size;
	int nblocks;
};



void rmem_init_sockets(struct rmem *r) {
	int sockfd;
    struct addrinfo hints, *servinfo, *p = NULL;
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "0", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }
    p = bindSocket(p, servinfo, &sockfd);
    r->targetIP = (struct sockaddr_in6 *) p->ai_addr;
    struct sockaddr_in6 *temp = (struct sockaddr_in6 *) r->targetIP;
    temp->sin6_port = htons(strtol("5000", (char **)NULL, 10));
    genPacketInfo(sockfd);
    openRawSocket();
	r->sockfd =sockfd;
}

void fill_rmem(struct rmem *r) {
	struct in6_addr *memList = malloc(sizeof(struct in6_addr) * r->nblocks);
	for (int i = 0; i<r->nblocks; i++){
		memList[i] = allocateRemoteMem(r->sockfd, r->targetIP);
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
	writeRemoteMem(r->sockfd, r->targetIP, data, &r->memList[block]);
}

void rmem_read( struct rmem *r, int block, char *data )
{
	if(block<0 || block>=r->nblocks) {
		fprintf(stderr,"disk_read: invalid block #%d\n",block);
		abort();
	}
	// Get pointer to page data in (simulated) physical memory
	memcpy(data, getRemoteMem(r->sockfd, r->targetIP, &r->memList[block]), r->block_size);

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
