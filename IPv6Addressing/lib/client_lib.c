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
struct in6_memaddr allocateRemoteMem(struct sockaddr_in6 *targetIP) {
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
 * Reads the remote memory based on remoteAddr
 */
// TODO: Implement meaningful return types and error messages
char *getRemoteMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    //int size = sizeof(GET_CMD);
    // Prep message
    //memcpy(sendBuffer, GET_CMD, size);
    remoteAddr->cmd =  GET_CMD;

    //memcpy(sendBuffer + size, remoteAddr, IPV6_SIZE);
    print_debug("******GET DATA******");
    // Send request and store response
    send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddr);
    rcv_udp6_raw_id(receiveBuffer,BLOCK_SIZE, targetIP, remoteAddr);
    return receiveBuffer;
}

/*
 * Sends a write command to the server based on remoteAddr
 */
// TODO: Implement meaningful return types and error messages
int writeRemoteMem(struct sockaddr_in6 *targetIP, char *payload, struct in6_memaddr *remoteAddr) {
    //int size = sizeof(WRITE_CMD);
    // Create the data
    //memcpy(sendBuffer, WRITE_CMD, size);

    remoteAddr->cmd =  WRITE_CMD;
    memcpy(sendBuffer, payload, BLOCK_SIZE);
    print_debug("******WRITE DATA******");
    send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddr);
    rcv_udp6_raw_id(receiveBuffer,BLOCK_SIZE, targetIP, remoteAddr);
    return EXIT_SUCCESS;
}

/*
 * Releases the remote memory based on remoteAddr
 */
// TODO: Implement meaningful return types and error messages
int freeRemoteMem(struct sockaddr_in6 *targetIP,  struct in6_memaddr *remoteAddr) {
    //int size = sizeof(FREE_CMD);
    // Create message
    //memcpy(sendBuffer, FREE_CMD, size);
    remoteAddr->cmd =  FREE_CMD;

    //memcpy(sendBuffer+size,remoteAddr,IPV6_SIZE);
    print_debug("******FREE DATA******");
    // Send message and check if it was successful
    // TODO: Check if it actually was successful
    send_udp6_raw(sendBuffer, BLOCK_SIZE, targetIP, remoteAddr);
    rcv_udp6_raw_id(receiveBuffer,BLOCK_SIZE, targetIP, remoteAddr);
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


