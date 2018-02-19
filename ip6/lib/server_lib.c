#define _GNU_SOURCE
#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <sys/mman.h>         // mmap()

#include "utils.h"
#include "network.h"
#include "config.h"

static __thread char sendBuffer[BLOCK_SIZE];

/*
 * TODO: explain.
 * Allocates local memory and exposes it to a client requesting it
 */
int allocate_mem(struct sockaddr_in6 *target_ip) {
    struct in6_memaddr allocPointer;
    //TODO: Error handling if we runt out of memory, this will fail
    //do some work, which might goto error
#ifdef SOCK_RAW
    void *allocated = calloc(1 ,BLOCK_SIZE);
#else
    void *allocated = rte_calloc(NULL, 1 ,BLOCK_SIZE, 64);
#endif
    memset(&allocPointer, 0, IPV6_SIZE);
    memcpy(&allocPointer.paddr, &allocated, POINTER_SIZE);
    memcpy(&allocPointer.subid, &SUBNET_ID, 2);

    //struct in6_addr r_addr; = getIPv6FromPointer((uint64_t) &allocated);
    memcpy(sendBuffer, "ACK", 3);
    memcpy(sendBuffer+3, &allocPointer, IPV6_SIZE); 
    send_udp_raw(sendBuffer, BLOCK_SIZE, target_ip);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Allocates local memory and exposes it to a client requesting it
 */
int allocate_mem_bulk( struct sockaddr_in6 *target_ip, uint64_t size) {
    struct in6_memaddr allocPointer;
    //TODO: Error handling if we runt out of memory, this will fail
    //do some work, which might goto error
    #ifdef SOCK_RAW
        void *allocated = calloc(size ,BLOCK_SIZE);
    #else
        void *allocated = rte_calloc(NULL, size ,BLOCK_SIZE, 64);
    #endif
    memset(&allocPointer, 0, IPV6_SIZE);
    memcpy(&allocPointer.paddr, &allocated, POINTER_SIZE);
    memcpy(&allocPointer.subid, &SUBNET_ID, 2);
    memcpy(sendBuffer, "ACK", 3);
    memcpy(sendBuffer+3, &allocPointer, IPV6_SIZE); 
    send_udp_raw(sendBuffer, BLOCK_SIZE, target_ip);
    return EXIT_SUCCESS;
}


/*
 * Gets memory and sends it
 */
int get_mem(struct sockaddr_in6 *target_ip, struct in6_memaddr *r_addr) {
    // Send the sendBuffer (entire BLOCK_SIZE)
    struct in6_memaddr *returnID = (struct in6_memaddr *) (&target_ip->sin6_addr);
    returnID->cmd = r_addr->cmd;
    returnID->paddr = r_addr->paddr;
    send_udp_raw((void *) *&r_addr->paddr, BLOCK_SIZE, target_ip);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int write_mem(char *receiveBuffer, struct sockaddr_in6 *target_ip, struct in6_memaddr *r_addr) {
    // Copy the first POINTER_SIZE bytes of receive buffer into the target
#ifdef SOCK_RAW
    memcpy((void *) *(&r_addr->paddr), receiveBuffer, BLOCK_SIZE); 
#else
    rte_memcpy((void *) *(&r_addr->paddr), receiveBuffer, BLOCK_SIZE);
#endif
    struct in6_memaddr *returnID = (struct in6_memaddr *) (&target_ip->sin6_addr);
    returnID->cmd = r_addr->cmd;
    returnID->paddr = r_addr->paddr;
    send_udp_raw("", 0, target_ip);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * This is freeing target memory?
 */
int free_mem(struct sockaddr_in6 *target_ip, struct in6_memaddr *r_addr) {
    //print_debug("Content stored at %p has been freed!", (void*)pointer);
    free((void *) *&r_addr->paddr);
    //munmap((void *) pointer, BLOCK_SIZE);
    memcpy(sendBuffer, "ACK", 3);
    struct in6_memaddr *returnID = (struct in6_memaddr *) (&target_ip->sin6_addr);
    returnID->cmd = r_addr->cmd;
    returnID->paddr = r_addr->paddr;
    send_udp_raw(sendBuffer, BLOCK_SIZE, target_ip);
    return EXIT_SUCCESS;
}
