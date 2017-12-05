/*
 ** client_lib.c -- 
 */

// TODO: Add a get remote machine method (needs to get the remote machine which holds a specific address.)
#include <string.h>           // strcpy, memset(), and memcpy()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <stdio.h>            // printf() and sprintf()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "client_lib.h"
#include "utils.h"

struct in6_addr *hostList;
int nhosts;
static __thread char sendBuffer[BLOCK_SIZE];
static __thread char receiveBuffer[BLOCK_SIZE];

/*
 * Generates a random IPv6 address target under specific constraints
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr *gen_rdm_IPv6Target() {
    // Pick a random host
    uint8_t rndHost = rand()% nhosts;
    return &hostList[rndHost];
}

/*Returns the IP of a given host*/
struct in6_addr *get_IPv6Target(uint8_t index) {
    return &hostList[index];
}

//return the number of hosts
int numHosts() {
    return nhosts;
}


/*
 * Generates a random IPv6 address target under specific constraints
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr *gen_IPv6Target(int offset) {
    // Pick a random host
    return &hostList[offset];
}

/*
 * Set the list of target hosts and their IP-adresses in our system.
 * Will be replaced by in-switch functionality.
 */
void set_host_list(struct in6_addr *host_addrs, int num_hosts) {
    hostList = host_addrs;
    nhosts = num_hosts;
}

/*
 * Allocate memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
struct in6_memaddr allocate_rmem(struct sockaddr_in6 *targetIP) {
    // Send the command to the target host and wait for response
    //memcpy(sendBuffer, ALLOC_CMD, sizeof(ALLOC_CMD));
    ((struct in6_memaddr *)&targetIP->sin6_addr)->cmd = ALLOC_CMD;

    send_udp_raw(sendBuffer, BLOCK_SIZE, targetIP);
    rcv_udp6_raw_id(receiveBuffer, BLOCK_SIZE, targetIP, NULL);
    struct in6_memaddr retAddr;
    print_debug("******ALLOCATE******");
    if (memcmp(receiveBuffer,"ACK", 3) == 0) {
        // If the message is ACK --> successful allocation
        // Copy the returned pointer (very precise offsets)
        memcpy(&retAddr, receiveBuffer+3, IPV6_SIZE);
        // Insert information about the source host (black magic)
        //00 00 00 00 01 02 00 00 00 00 00 00 00 00 00 00
        //            ^  ^ these two bytes are stored (subnet and host ID)
        memcpy(&retAddr.subid, ((char*)&targetIP->sin6_addr)+4, 2);
    } else {
        perror("Response was not successful\n");
        // Not successful set the return address to zero.
        memset(&retAddr,0, IPV6_SIZE);
    }
/*    char dst_ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6,&retAddr, dst_ip, sizeof dst_ip);
    print_debug("Allocated %s", dst_ip);*/
    return retAddr;
}

/*
 * Allocates a bulk of memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
struct in6_memaddr *allocate_rmem_bulk(struct sockaddr_in6 *targetIP, uint64_t size) {
    // Send the command to the target host and wait for response
    memcpy(sendBuffer, &size, sizeof(uint64_t));
    ((struct in6_memaddr *)&targetIP->sin6_addr)->cmd = ALLOC_BULK_CMD;

    send_udp_raw(sendBuffer, BLOCK_SIZE, targetIP);
    rcv_udp6_raw_id(receiveBuffer, BLOCK_SIZE, targetIP, NULL);
    struct in6_memaddr *addrList = malloc(size * sizeof(struct in6_memaddr));
    struct in6_memaddr retAddr;
    print_debug("******ALLOCATE******");
    if (memcmp(receiveBuffer,"ACK", 3) == 0) {
        // If the message is ACK --> successful allocation
        // Copy the returned pointer (very precise offsets)
        memcpy(&retAddr, receiveBuffer+3, IPV6_SIZE);
        // Insert information about the source host (black magic)
        //00 00 00 00 01 02 00 00 00 00 00 00 00 00 00 00
        //            ^  ^ these two bytes are stored (subnet and host ID)
        memcpy(&retAddr.subid, ((char*)&targetIP->sin6_addr)+4, 2);

    } else {
        perror("Response was not successful\n");
        // Not successful set the return address to zero.
        memset(&retAddr,0, IPV6_SIZE);
    }
    // Convert the returned pointer into an array of pointers
    for (uint64_t i = 0; i < size; i++) {
        addrList[i] = retAddr;
        addrList[i].paddr = retAddr.paddr + i * BLOCK_SIZE;

    }
    return addrList;
}



/*
 * Reads the remote memory based on remoteAddr
 */
// TODO: Implement meaningful return types and error messages
char *get_rmem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    //int size = sizeof(GET_CMD);
    // Prep message
    //memcpy(sendBuffer, GET_CMD, size);
    remoteAddr->cmd =  GET_CMD;
    //memcpy(sendBuffer + size, remoteAddr, IPV6_SIZE);
    print_debug("******GET DATA******");
    // Send request and store response
    do
        send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddr);
    while (rcv_udp6_raw_id(receiveBuffer,BLOCK_SIZE, targetIP, remoteAddr) < 0);
    return receiveBuffer;
}

/*
 * Sends a write command to the server based on remoteAddr
 */
// TODO: Implement meaningful return types and error messages
int write_rmem(struct sockaddr_in6 *targetIP, char *payload, struct in6_memaddr *remoteAddr) {
    //int size = sizeof(WRITE_CMD);
    // Create the data
    //memcpy(sendBuffer, WRITE_CMD, size);

    remoteAddr->cmd =  WRITE_CMD;
    print_debug("Writing to send buffer");
    memcpy(sendBuffer, payload, BLOCK_SIZE);
    print_debug("******WRITE DATA******");
    do
        send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddr);
    while (rcv_udp6_raw_id(receiveBuffer,BLOCK_SIZE, targetIP, remoteAddr) < 0);
    return EXIT_SUCCESS;
}

//Returns the missing raid read -1 if everything is read
int readRaidMem(struct sockaddr_in6 *targetIP, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed) {
    int host;
    int found[MAX_HOSTS];
    for (int i=0;i<hosts;i++) {
        found[i] = 0;
    }
    //memcpy(sendBuffer + size, remoteAddr, IPV6_SIZE);
    print_debug("******GET DATA******");
    for (int i=0; i<hosts;i++) {
        remoteAddrs[i]->cmd =  GET_CMD;
        send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddrs[i]);
    }
    for (int i=0; i <hosts;i++) {
        //TODO check a list to ensure that the correct messages are
        //being acked
        //TODO timeout or something if a failure occurs here
        int bytes = rcv_udp6_raw_id(receiveBuffer, BLOCK_SIZE, targetIP, NULL);
        //take care of timeout
        if ( bytes == -1 && i >= needed) {
            //There is no point in reading another page if the timeout
            //occured, just break
            break;
        } else if (bytes == -1 && i < needed) {
            //In this case a timeout occured, but there is
            //insufficient data to repair a page, so another read must
            //be done.
            //TODO do retransmission of the request to the server.
            i--;
            continue;
        } else {
            //Here a page was actually read
            host = (int)targetIP->sin6_addr.s6_addr[5]-2;
            //printf("%d\n",host);
            //printf("rec :%s\n",receiveBuffer);
            //printf("Read From %d\n",(int)targetIP->sin6_addr.s6_addr[5]);
            memcpy(&((*bufs)[host]),receiveBuffer,BLOCK_SIZE);
            //printf("cpy :%s\n",(*bufs)[host]);
            //printf("copied\n");
            //printf("%d ",host);
            found[host] = 1;
        }
    }
    for (int i=0;i<hosts;i++) {
        if (found[i] == 0) {
            return i;
        }
    }
    return -1;
}

int writeRaidMem(struct sockaddr_in6 *targetIP, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed) {
    int host;

    for (int i=0; i <hosts;i++) {
        //printf("sending write request packet %d\n",i);
        remoteAddrs[i]->cmd = WRITE_CMD;
        //printf("Sending Page UDP\n");
        send_udp6_raw((char*)&((*payload)[i]), BLOCK_SIZE, targetIP, remoteAddrs[i]);
        //printf("FINISHED sending write request packet %d\n",i);
    }
    for (int i=0; i <hosts;i++) {
        //printf("receiving %d\n",i);
        //TODO check a list to ensure that the correct messages are
        //being acked
        //TODO timeout or something if a failure occurs here
        //printf("reading write ACK request packet %d\n",i);
        
        int bytes = rcv_udp6_raw_id(receiveBuffer, BLOCK_SIZE, targetIP, NULL);
        if ( bytes == -1 && i >= needed) {
            //There is no point in reading another page if the timeout
            //occured, just break
            break;
        } else if (bytes == -1 && i < needed) {
            //In this case a timeout occured, but there is
            //insufficient data to repair a page, so another read must
            //be done.
            //TODO do retransmission of the request to the server.
            i--;
            continue;
        } else {
            host = (int)targetIP->sin6_addr.s6_addr[5];
            //printf("Read From %d\n",(int)targetIP->sin6_addr.s6_addr[5]);
            memcpy(&((*payload)[host-2]),receiveBuffer,BLOCK_SIZE);
        }
    }
    return EXIT_SUCCESS;
}



/*
 * Releases the remote memory based on remoteAddr
 */
// TODO: Implement meaningful return types and error messages
int free_rmem(struct sockaddr_in6 *targetIP,  struct in6_memaddr *remoteAddr) {
    //int size = sizeof(FREE_CMD);
    // Create message
    //memcpy(sendBuffer, FREE_CMD, size);
    remoteAddr->cmd =  FREE_CMD;

    //memcpy(sendBuffer+size,remoteAddr,IPV6_SIZE);
    print_debug("******FREE DATA******");
    // Send message and check if it was successful
    // TODO: Check if it actually was successful
    do
        send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddr);
    while (rcv_udp6_raw_id(receiveBuffer,BLOCK_SIZE, targetIP, remoteAddr) < 0);
    return EXIT_SUCCESS;
}

/*
 * Migrates remote memory
 */
//TODO: Currently unused.
/*int migrateRemoteMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int machineID) {
    // Allocates storage
    char ovs_cmd[100];
    char s[INET6_ADDRSTRLEN];
    inet_ntop(targetIP->sin6_family,((struct in6_addr *) remoteAddr)->s6_addr, s, sizeof s);
    printf("Getting pointer\n");
    memcpy(receiveBuffer, getRemoteMem(targetIP, remoteAddr), BLOCK_SIZE);
    printf("Freeing pointer\n");
    freeRemoteMem(targetIP, remoteAddr);
    printf("Writing pointer\n");
    sprintf(ovs_cmd, "ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dst=%s,priority=65535,actions=output:%d", s, machineID);
    int status = system(ovs_cmd);
    printf("%d\t%s\n", status, ovs_cmd);
    writeRemoteMem(targetIP, receiveBuffer, remoteAddr);
    return EXIT_SUCCESS;
}*/


