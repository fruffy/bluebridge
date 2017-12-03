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
#include <assert.h>

#include "../client_lib.h"
#include "rmem.h"
#include "rrmem.h"
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
    myConf = set_bb_config(filename, 0);
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
    r->raid=4;
    r->block_size = BLOCK_SIZE;
    r->nblocks = nblocks;
    fill_rrmem(r);
    return r;
}

char wbufs [MAX_HOSTS][BLOCK_SIZE];
struct in6_memaddr *wremoteAddrs[MAX_HOSTS];
void rrmem_write(struct rrmem *r, int block, char *data ) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"rr_write: invalid block #%d\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    // Slice up data based on given raid configuration
    //char parity [PAGE_SIZE];
    switch (r->raid) {
        case 4 :
            //printf("Writing Raid %d\n",r->raid);
            assert(numHosts() >= 3 && numHosts() <= MAX_HOSTS);
            int alloc = r->block_size / (numHosts() - 1); //Raid 4
            if (r->block_size % (numHosts() -1) > 0) {
                alloc++;
            }
            int base = 0;
            //struct in6_memaddr **remoteAddrs = malloc(sizeof(struct in6_memaddr*) * numHosts());
            //calculate parity
            parity45(data,r->block_size,numHosts()-1,(char *)&wbufs[numHosts()-1]);
            for (int i = 0; i<numHosts()-1; i++) {
                if (r->block_size % (numHosts() -1) <= i) {
                    alloc = r->block_size / (numHosts() -1);
                }
                //printf("Copying read data to buffers\n");
                memcpy(&wbufs[i],&(data[base]),alloc);
                //printf("Copying Remote Adders\n");
                wremoteAddrs[i] = &r->rsmem[i].memList[block];
                base += alloc;
            }
            wremoteAddrs[numHosts()-1] = &r->rsmem[numHosts()-1].memList[block];
            //printf("Writing with parallel raid\n");
            //printf("Writing Remote\n");
            writeRaidMem(r->targetIP,numHosts(),&wbufs, (struct in6_memaddr**)wremoteAddrs,numHosts());
            //printf("FINISHED :: Writing with parallel raid\n");
            break;


        default:
            printf("Raid %d not implemented FATAL write request\n",r->raid);
            //TODO Free memory and exit
            exit(0);
            break;
    }
}

char rbufs [MAX_HOSTS][BLOCK_SIZE];
struct in6_memaddr *rremoteAddrs[MAX_HOSTS];
void rrmem_read( struct rrmem *r, int block, char *data ) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%d\n",block);
        abort();
    }
    switch (r->raid) {
        case 4:
            assert(numHosts() >= 3 && numHosts() <= MAX_HOSTS);
            int missingIndex;
            for (int i = 0; i<numHosts() ; i++) {
                //remoteReads[i] = malloc(sizeof(char) * r->block_size);
                rremoteAddrs[i] = &r->rsmem[i].memList[block];
            }
            //printf("Reading Remotely (Parallel Raid)\n");

            missingIndex = readRaidMem(r->targetIP,numHosts(),&rbufs,(struct in6_memaddr**)rremoteAddrs,numHosts());
            //missingIndex = readRaidMem(r->targetIP,numHosts(),remoteReads,remoteAddrs,numHosts() - 1);
            if (missingIndex == -1) {
                //printf("All stripes retrieved checking for correctness\n");
                if (!checkParity45(&rbufs,numHosts()-1,&(rbufs[numHosts()-1]),r->block_size)) {
                    //TODO reissue requests and try again, or correct the
                    //broken page
                    printf("Parity Not correct!!! Crashing");
                    //exit(1);
                }
            } else if (missingIndex == (numHosts() - 1)) {
                printf("parity missing, just keep going without correctness\n");
            } else {
                printf("missing page %d, repairing from parity\n", missingIndex);
                repairStripeFromParity45(&rbufs[missingIndex],&rbufs,&rbufs[numHosts()-1],missingIndex,numHosts()-1,r->block_size);
                printf("repair complete\n");
            }

            int alloc = r->block_size / (numHosts() - 1); //Raid 4
            if (r->block_size % (numHosts() -1) > 0) {
                alloc++;
            }
            int base = 0;
            for (int i = 0; i<numHosts()-1; i++) {
                if (r->block_size % (numHosts() -1) <= i) {
                    alloc = r->block_size / (numHosts() -1);
                }
                memcpy(&(data[base]),&(rbufs[i]),alloc);
                base += alloc;
            }
            printf("PAGE:\n%s\n",data);


            //Temporary, recovery mechanism, attempt to rebuild a page
            //in the case that it is missing
            /*
            char * repair = malloc(sizeof(char) * r->block_size);
            char * dupData = malloc(sizeof(char) * r->block_size);
            int missing = 2;

            repairstripefromparity45(repair,remotereads,remotereads[numhosts()-1],missing,numhosts()-1,r->block_size);
            remotereads[missing] = repair;
            //TODO remove total copy
            alloc = r->block_size / (numHosts() - 1); //Raid 4
            if (r->block_size % (numHosts() -1) > 0) {
                alloc++;
            }
            base = 0;
            for (int i = 0; i<numHosts()-1; i++) {
                if (r->block_size % (numHosts() -1) <= i) {
                    alloc = r->block_size / (numHosts() -1);
                }
                memcpy(&(dupData[base]),remoteReads[i],alloc);
                base += alloc;
            }
            //FINAL TEMPORYARY Compare between rep, and orig
            if (!memcmp(data,dupData,r->block_size) == 0 ){
                printf("REPAIR FAILED\n");
                exit(0);
            } else {
                print_debug("Integrity Passed\n");
            }
            */


            //printf("Finished Parallel Raid Read\n");
            break;
        
        default:
            printf("Raid %d not implemented FATAL read request\n",r->raid);
            break;
    }


}

//stripes are assumed to be ordered
void repairStripeFromParity45(char (*repair)[BLOCK_SIZE], char (*stripes)[MAX_HOSTS][BLOCK_SIZE], char (*parity)[BLOCK_SIZE], int missing, int numStripes, int size) {
    int alloc = size / numStripes;
    for (int i=0; i< alloc; i++) {
        char repairbyte = 0;
        for (int j=0;j<numStripes;j++) {
            //this may seem like inverted access but i'm checking
            //across stripes
            if ( j != missing ) {
                repairbyte = repairbyte ^ (*stripes)[j][i];
            }
        }
        (*repair)[i] = repairbyte ^ (*parity)[i];
    }
    //This is only called if the page size does not evenly divide
    //across strips, and the missing page is one of the larger stripes
    if ( size % numStripes > missing) {
        char repairbyte = 0;
        for (int i=0; i<size%numStripes;i++) {
            if ( i != missing)  {
                repairbyte = repairbyte ^ (*stripes)[i][alloc];
            }
        }
        (*repair)[alloc] = repairbyte ^ (*parity)[alloc];
    }
    return;
    
}


void parity45(char *data, int size, int stripes, char *parity) {
    //Malloc the correct ammount of space for the parity
    int alloc = size / stripes;
    /*
    char * parity;
    if (size %stripes > 0) {
        parity = malloc(sizeof(char) * (alloc + 1));
    } else {
        parity = malloc(sizeof(char) * alloc);
    }*/

    //calculate the majority of the parity (final byte may not be
    //calculated
    for (int i=0; i< alloc; i++) {
        char paritybyte = 0;
        int base = 0;
        for (int j=0;j<stripes;j++) {
            paritybyte = paritybyte ^ data[i + base];
            /*
            if (i == 0) {
                printf("PARITY i:%d base:%d byte:%02x TransParity:%02x \n",i, base ,data[i + base],paritybyte);
            }
            */
            base += alloc;
            if (j < size % stripes) {
                base++;
            }
        }
        parity[i] = paritybyte;
    }
    //printf("final parody\n");
    if (size%stripes > 0) {
        char paritybyte = 0;
        int base = alloc;
        for (int i=0; i<size%stripes;i++) {
            paritybyte = paritybyte ^ data[base];
            //printf("PARITY i:%d base:%d byte:%02x TransParity:%02x \n",i, base ,data[base],paritybyte);
            base += alloc + 1;
        }
        parity[alloc] = paritybyte;
    }
    return;
}

//Here stripes are assumed to be in order from biggest to smallest
int checkParity45(char (*stripes)[MAX_HOSTS][BLOCK_SIZE], int numStripes, char (*parity)[BLOCK_SIZE], int size) {
    int alloc = size / numStripes;
    for (int i=0; i< alloc; i++) {
        char paritybyte = 0;
        //int base = 0;
        for (int j=0;j<numStripes;j++) {
            //this may seem like inverted access but i'm checking
            //across stripes
            paritybyte = paritybyte ^ (*stripes)[j][i];
            /*
            if (i == 0) {
                printf("CHECK index :%d stripe:%d byte:%02x TransParity:%02x CompParity:%02x \n",j, i ,stripes[j][i],paritybyte,parity[i]);
            }
            */
        }
        if (paritybyte != (*parity)[i]) {
            printf("Fault on byte %d : Calculated Parity Byte %02x != %02x written parity\n",i,paritybyte,(*parity)[i]);
            return 0;
        }
    }
    //This is only called if the page size does not evenly divide
    //across strips, it checks the parody of 
    //printf("final parody check\n");
    if ( size % numStripes > 0) {
        char paritybyte = 0;
        for (int i=0; i<size%numStripes;i++) {
            //printf("CHECK index:%d stripe:%d byte:%02x TransParity:%02x \n",alloc, i ,stripes[i][alloc],paritybyte);
            paritybyte = paritybyte ^ (*stripes)[i][alloc];
        }
        if (paritybyte != (*parity)[alloc]) {
            printf("Fault on final byte %d : Calculated Parity Byte %02x != %02x written parity\n",alloc,paritybyte,parity[alloc]);
            return 0;
        }
    }
    //The parity has been verified
    return 1;
}





int rrmem_blocks(struct rrmem *r) {
    return r->nblocks;
}

void rrmem_deallocate(struct rrmem *r) {
    for(int i =0; i<numHosts();i++){
        for (int j = 0; j<r->nblocks; j++){
            freeRemoteMem(r->targetIP, &r->rsmem[i].memList[j]);
        }
    }
    close_sockets();
    free(r);
}
