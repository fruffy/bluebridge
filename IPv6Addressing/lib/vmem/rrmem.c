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
#include "rmem.h"
#include "../utils.h"

extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off_t __offset);
extern ssize_t pwrite (int __fd, const void *__buf, size_t __nbytes, __off_t __offset);

//trrmem is a temporary implementation of raid memory
struct rrmem {
    struct sockaddr_in6 *targetIP;
    struct rmem *rsmem; //a striped array of remote memory, one for each host
    int raid;    //the raid level implemented
    int block_size;
    int nblocks;
};


struct config myConf;
void configure_rrmem(char *filename) {
    myConf = configure_bluebridge(filename, 0);
    set_host_list(myConf.hosts, myConf.num_hosts);
}

void rrmem_init_sockets(struct rrmem *r) {
    r->targetIP = init_rcv_socket(&myConf);
    r->targetIP->sin6_port = htons(strtol(myConf.server_port, (char **)NULL, 10));
    init_send_socket(&myConf);
}


void fill_rrmem(struct rrmem *r) {
    struct rmem *hostlist = malloc(sizeof(struct rmem) * numHosts());
    for (int i = 0; i<numHosts(); i++) {
        struct in6_memaddr *memList = malloc(sizeof(struct in6_addr) * r->nblocks);
        struct in6_addr *ipv6Pointer = get_IPv6Target(i);
        memcpy(&(r->targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));
        fprintf(stdout,"Target IP %x\n",r->targetIP->sin6_addr);
        for (int j = 0; j<r->nblocks; j++) {
            memList[j] = allocateRemoteMem(r->targetIP);
        }
        hostlist[i].memList = memList;
    }
    r->rsmem = hostlist;
}

struct rrmem *rrmem_allocate(int nblocks) {
    //allocate space for raided memeory
    struct rrmem *r;
    r = malloc(sizeof(*r));
    if(!r) return 0;

    //initalize soccet connection
    rrmem_init_sockets(r);
    r->block_size = BLOCK_SIZE;
    r->nblocks = nblocks;
    fill_rrmem(r);
    return r;
}

void rrmem_write(struct rrmem *r, int block, char *data ) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"rr_write: invalid block #%d\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    // Slice up data based on given raid configuration
    int alloc = r->block_size / (numHosts() - 1); //Raid 4
    if (r->block_size % (numHosts() -1) > 0) {
        alloc++;
    }
    int base = 0;
    char *buf = malloc(sizeof(char) * alloc);
    printf("Writing to Remote hosts\n");
    for (int i = 0; i<numHosts()-1; i++) {
        if (r->block_size % (numHosts() -1) <= i) {
            alloc = r->block_size / (numHosts() -1);
        }
        printf("Writing %d bytes of Data to host %d from loc %d\n",alloc,i,base);
        memcpy(buf,&(data[base]),alloc);
        writeRemoteMem(r->targetIP, buf, &r->rsmem[i].memList[block]);
        base += alloc;
    }
}

void rrmem_read( struct rrmem *r, int block, char *data ) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%d\n",block);
        abort();
    }
    char **remoteReads = malloc(sizeof(char*) * numHosts());
    for (int i = 0; i<numHosts(); i++) {
        remoteReads[i] = malloc(sizeof(char) * r->block_size);
        // Get pointer to page data in (simulated) physical memory
        //printf("memlist [cmd:%d] \n",&r->rsmem[i].memList[block].cmd);
        //printf("memlist [subid:%d] \n",&r->rsmem[i].memList[block].subid);
        //printf("memlist [paddr:%d] \n",&r->rsmem[i].memList[block].paddr);
        memcpy(remoteReads[i], getRemoteMem(r->targetIP, &r->rsmem[i].memList[block]), r->block_size);
    }
    /*
    for (int i= 0; i < numHosts() -1;i++) {
        if( memcmp(remoteReads[i],remoteReads[i+1],r->block_size) != 0) {
            printf("Inconsistant pages [%s]\n, [%s]\n",remoteReads[i],remoteReads[i+1]);
        }
    }*/
    int alloc = r->block_size / (numHosts() - 1); //Raid 4
    if (r->block_size % (numHosts() -1) > 0) {
        alloc++;
    }
    int base = 0;
    for (int i = 0; i<numHosts()-1; i++) {
        if (r->block_size % (numHosts() -1) <= i) {
            alloc = r->block_size / (numHosts() -1);
        }
        memcpy(&(data[base]),remoteReads[i],alloc);
        base += alloc;
    }

}

int rrmem_blocks(struct rrmem *r) {
    return r->nblocks;
}

void rrmem_deallocate(struct rrmem *r) {
    for (int i = 0; i<r->nblocks; i++){
        freeRemoteMem(r->targetIP, &r->rsmem[0].memList[i]);
    }
    close_sockets();
    free(r);
}
