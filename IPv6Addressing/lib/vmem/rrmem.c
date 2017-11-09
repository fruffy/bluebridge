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

void rrmem_write(struct rrmem *r, int block, char *data ) {
    /*
    for (int i=0;i<r->block_size;i++){
        printf("%02x",data[i]);
    }*/
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"rr_write: invalid block #%d\n",block);
        abort();
    }
    // Get pointer to page data in (simulated) physical memory
    // Slice up data based on given raid configuration
    switch (r->raid) {
        case 4:
            assert(numHosts() >= 3);
            int alloc = r->block_size / (numHosts() - 1); //Raid 4
            if (r->block_size % (numHosts() -1) > 0) {
                alloc++;
            }
            int base = 0;
            char *buf = malloc(sizeof(char) * r->block_size);
            //printf("Writing to Remote hosts\n");
            char * parity = parity45(data,r->block_size,numHosts()-1);
            memcpy(buf,parity,alloc);
            /*
            printf("-----------------------\n");
            for (int i =0; i< alloc;i++) {
                printf("%02X ",buf[i]);
                if (i > 8) {
                    printf("\n");
                    break;
                }
            }
            printf("-----------------------\n");
            printf("\n");
            */
            for (int i = 0; i<numHosts()-1; i++) {
                //printf("slice %d\n",i);
                if (r->block_size % (numHosts() -1) <= i) {
                    alloc = r->block_size / (numHosts() -1);
                }
                //printf("Writing %d bytes of Data to host %d from loc %d\n",alloc,i,base);
                memcpy(buf,&(data[base]),alloc);
                /*
                for (int i =0; i< alloc;i++) {
                    printf("%02X ",buf[i]);
                    if (i > 8) {
                        printf("\n");
                        break;
                    }
                }*/
                writeRemoteMem(r->targetIP, buf, &r->rsmem[i].memList[block]);
                base += alloc;
            }
            //Write out parity
            //Get max len alloc again
            alloc = r->block_size / (numHosts() - 1); //Raid 4
            if (r->block_size % (numHosts() -1) > 0) {
                alloc++;
            }
            memcpy(buf,parity,alloc);
            writeRemoteMem(r->targetIP,buf, &r->rsmem[numHosts() -1].memList[block]);
            //printf("\n\n\n");

            break;
        default:
            printf("Raid %d not implemented FATAL write request\n",r->raid);
            //TODO Free memory and exit
            exit(0);
            break;
    }
}

void rrmem_read( struct rrmem *r, int block, char *data ) {
    if(block<0 || block>=r->nblocks) {
        fprintf(stderr,"disk_read: invalid block #%d\n",block);
        abort();
    }
    switch (r->raid) {
        case 4:
            assert(numHosts() >= 3);
            char **remoteReads = malloc(sizeof(char*) * numHosts() - 1 );
            for (int i = 0; i<numHosts() -1 ; i++) {
                remoteReads[i] = malloc(sizeof(char) * r->block_size);
                memcpy(remoteReads[i], getRemoteMem(r->targetIP, &r->rsmem[i].memList[block]), r->block_size);
            }
            char *parity = malloc(sizeof(char) * r->block_size);
            memcpy(parity, getRemoteMem(r->targetIP, &r->rsmem[numHosts()-1].memList[block]), r->block_size);
            if (!checkParity45(remoteReads,numHosts()-1,parity,r->block_size)) {
                //TODO reissue requests and try again, or correct the
                //broken page
                printf("Parity Not correct!!! Crashing");
                exit(1);
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
                memcpy(&(data[base]),remoteReads[i],alloc);
                base += alloc;
            }
            //Temporary, recovery mechanism, attempt to rebuild a page
            //in the case that it is missing
            char * repair = malloc(sizeof(char) * r->block_size);
            char * dupData = malloc(sizeof(char) * r->block_size);
            int missing = 2;

            repairStripeFromParity45(repair,remoteReads,parity,missing,numHosts()-1,r->block_size);
            remoteReads[missing] = repair;
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
            }

            break;
        default:
            printf("Raid %d not implemented FATAL read request\n",r->raid);
            break;
    }


}

//stripes are assumed to be ordered
void repairStripeFromParity45(char* repair, char **stripes, char *parity, int missing, int numStripes, int size) {
    int alloc = size / numStripes;
    for (int i=0; i< alloc; i++) {
        char repairbyte = 0;
        for (int j=0;j<numStripes;j++) {
            //this may seem like inverted access but i'm checking
            //across stripes
            if ( j != missing ) {
                repairbyte = repairbyte ^ stripes[j][i];
            }
        }
        repair[i] = repairbyte ^ parity[i];
    }
    //This is only called if the page size does not evenly divide
    //across strips, and the missing page is one of the larger stripes
    if ( size % numStripes > missing) {
        char repairbyte = 0;
        for (int i=0; i<size%numStripes;i++) {
            if ( i != missing)  {
                repairbyte = repairbyte ^ stripes[i][alloc];
            }
        }
        repair[alloc] = repairbyte ^ parity[alloc];
    }
    return;
    
}


char * parity45(char *data, int size, int stripes) {
    //Malloc the correct ammount of space for the parity
    int alloc = size / stripes;
    char * parity;
    if (size %stripes > 0) {
        parity = malloc(sizeof(char) * (alloc + 1));
    } else {
        parity = malloc(sizeof(char) * alloc);
    }

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
    return parity;
}

//Here stripes are assumed to be in order from biggest to smallest
int checkParity45(char **stripes, int numStripes, char * parity, int size) {
    int alloc = size / numStripes;
    for (int i=0; i< alloc; i++) {
        char paritybyte = 0;
        int base = 0;
        for (int j=0;j<numStripes;j++) {
            //this may seem like inverted access but i'm checking
            //across stripes
            paritybyte = paritybyte ^ stripes[j][i];
            /*
            if (i == 0) {
                printf("CHECK index :%d stripe:%d byte:%02x TransParity:%02x CompParity:%02x \n",j, i ,stripes[j][i],paritybyte,parity[i]);
            }
            */
        }
        if (paritybyte != parity[i]) {
            printf("Fault on byte %d : Calculated Parity Byte %02x != %02x written parity\n",i,paritybyte,parity[i]);
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
            paritybyte = paritybyte ^ stripes[i][alloc];
        }
        if (paritybyte != parity[alloc]) {
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
    for (int i = 0; i<r->nblocks; i++){
        freeRemoteMem(r->targetIP, &r->rsmem[0].memList[i]);
    }
    close_sockets();
    free(r);
}
