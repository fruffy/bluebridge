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

#ifndef DEFAULT
#include <rte_ethdev.h>       // main DPDK library
#include <rte_malloc.h>       // rte_zmalloc_socket()
#endif

struct in6_addr *hostList;
int nhosts;
static char sendBuffer[BLOCK_SIZE];

/*
 * Generates a random IPv6 address target under specific constraints
 * This is used by the client to request a pointer from a random server in the network
 * In future implementations this will be handled by the switch and controller to 
 * loadbalance. The client will send out a generic request. 
 */
struct in6_addr *gen_rdm_ip6_target() {
    // Pick a random host
    uint8_t rndHost = rand()% nhosts;
    return &hostList[rndHost];
}

/*Returns the IP of a given host*/
struct in6_addr *get_ip6_target(uint8_t index) {
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
struct in6_addr *gen_ip6_target(int offset) {
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
    char receiveBuffer[BLOCK_SIZE];

    // Send the command to the target host and wait for response
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
    return retAddr;
}

/*
 * Allocates a bulk of memory from a remote machine.
 */
// TODO: Implement error handling, struct in6_addr *  retVal is passed as pointer into function and we return int error codes
struct in6_memaddr *allocate_rmem_bulk(struct sockaddr_in6 *targetIP, uint64_t size) {
    char receiveBuffer[BLOCK_SIZE];
    // Send the command to the target host and wait for response
    memcpy(sendBuffer, &size, sizeof(uint64_t));
    ((struct in6_memaddr *)&targetIP->sin6_addr)->cmd = ALLOC_BULK_CMD;
    send_udp_raw(sendBuffer, BLOCK_SIZE, targetIP);
    rcv_udp6_raw_id(receiveBuffer, BLOCK_SIZE, targetIP, NULL);
    struct in6_memaddr *addrList = malloc(size * sizeof(struct in6_memaddr));
    struct in6_memaddr retAddr;
    print_debug("******ALLOCATE BULK******");
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
int get_rmem(char *receiveBuffer, int length, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
// #ifndef SOCK_RAW
//     char *receiveBuffer = rte_malloc(NULL, BLOCK_SIZE, 64);
// #else
//     char *receiveBuffer = malloc(BLOCK_SIZE);
// #endif
    // Send the command to the target host and wait for response
    remoteAddr->cmd =  GET_CMD;
    print_debug("******GET DATA******");
    // Send request and store response
    send_udp6_raw(sendBuffer, 0, targetIP, remoteAddr);
    int numBytes = rcv_udp6_raw_id(receiveBuffer, length, targetIP, remoteAddr);
    return numBytes;
}

/*
 * Sends a write command to the server based on remoteAddr
 */
// TODO: Implement meaningful return types and error messages
int write_rmem(struct sockaddr_in6 *targetIP, char *payload, struct in6_memaddr *remoteAddr) {
    // Send the command to the target host and wait for response
    // printf("Writing things\n");
    remoteAddr->cmd =  WRITE_CMD;
    print_debug("******WRITE DATA******");
    send_udp6_raw(payload, BLOCK_SIZE, targetIP, remoteAddr);
    // printf("receive \n");
    rcv_udp6_raw_id(NULL, 0, targetIP, remoteAddr);
    // printf("returning\n");
    return EXIT_SUCCESS;
}

//Returns the missing raid read -1 if everything is read
int read_raid_mem(struct sockaddr_in6 *targetIP, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed) {
    int host;
    char receiveBuffer[BLOCK_SIZE];

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

int write_raid_mem(struct sockaddr_in6 *targetIP, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed) {
    int host;
    char receiveBuffer[BLOCK_SIZE];

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
    char receiveBuffer[BLOCK_SIZE];
    // Create message
    remoteAddr->cmd =  FREE_CMD;
    print_debug("******FREE DATA******");
    // Send message and check if it was successful
    // TODO: Check if it actually was successful
    send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddr);
    rcv_udp6_raw_id(receiveBuffer,BLOCK_SIZE, targetIP, remoteAddr);
    return EXIT_SUCCESS;
}

