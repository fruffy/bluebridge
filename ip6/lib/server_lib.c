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
    //ip6_memaddr allocPointer;
    //TODO: Error handling if we runt out of memory, this will fail
    //do some work, which might goto error
#ifdef SOCK_RAW
    void *allocated = calloc(1 ,BLOCK_SIZE);
#else
    void *allocated = rte_calloc(NULL, 1 ,BLOCK_SIZE, 64);
#endif
    ip6_memaddr *returnID = (ip6_memaddr *)&target_ip->sin6_addr;
    returnID->cmd = ALLOC_CMD;
    memcpy(&returnID->paddr, &allocated, POINTER_SIZE);
    send_udp_raw(sendBuffer, BLOCK_SIZE, (ip6_memaddr *)&target_ip->sin6_addr, target_ip->sin6_port);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Allocates local memory and exposes it to a client requesting it
 */
int allocate_mem_bulk( struct sockaddr_in6 *target_ip, uint64_t size) {
    //ip6_memaddr allocPointer;
    //TODO: Error handling if we runt out of memory, this will fail
    //do some work, which might goto error
    if (!size)
        size = 1;
    #ifdef SOCK_RAW
        void *allocated = calloc(size, BLOCK_SIZE);
    #else
        void *allocated = rte_calloc(NULL, size, BLOCK_SIZE, 64);
    #endif
    ip6_memaddr *returnID = (ip6_memaddr *)&target_ip->sin6_addr;
    returnID->cmd = ALLOC_BULK_CMD;
    memcpy(&returnID->paddr, &allocated, POINTER_SIZE);
    send_udp_raw(sendBuffer, BLOCK_SIZE, (ip6_memaddr *)&target_ip->sin6_addr, target_ip->sin6_port);
    return EXIT_SUCCESS;
}


/*
 * Gets memory and sends it
 */
int read_mem(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr) {
    // Send the sendBuffer (entire BLOCK_SIZE)
    ip6_memaddr *returnID = (ip6_memaddr *) (&target_ip->sin6_addr);
    returnID->cmd = r_addr->cmd;
    returnID->paddr = r_addr->paddr;
    send_udp_raw((void *) *&r_addr->paddr, BLOCK_SIZE, (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * Gets memory and sends it
 */
int read_mem_ptr(char *receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr) {
    // Send the sendBuffer (entire BLOCK_SIZE)
    ip6_memaddr *returnID = (ip6_memaddr *) (&target_ip->sin6_addr);
    returnID->cmd = r_addr->cmd;
    memcpy(&returnID->paddr, receiveBuffer, POINTER_SIZE);
    send_udp_raw((void *) *&r_addr->paddr, BLOCK_SIZE, (ip6_memaddr *) &target_ip->sin6_addr, target_ip->sin6_port);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}


/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int write_mem(char *receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr) {
    // Copy the first POINTER_SIZE bytes of receive buffer into the target
#ifdef SOCK_RAW
    memcpy((void *) *(&r_addr->paddr), receiveBuffer, BLOCK_SIZE); 
#else
    rte_memcpy((void *) *(&r_addr->paddr), receiveBuffer, BLOCK_SIZE);
#endif
    //__sync_synchronize();
    ip6_memaddr *returnID = (ip6_memaddr *) (&target_ip->sin6_addr);
    returnID->cmd = r_addr->cmd;
    returnID->paddr = r_addr->paddr;
    send_udp_raw("", 0, (ip6_memaddr *)&target_ip->sin6_addr, target_ip->sin6_port);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int write_mem_bulk(char *receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr) {
    // Copy the first POINTER_SIZE bytes of receive buffer into the target
#ifdef SOCK_RAW
    memcpy((void *) *(&r_addr->paddr), receiveBuffer, BLOCK_SIZE); 
#else
    rte_memcpy((void *) *(&r_addr->paddr), receiveBuffer, BLOCK_SIZE);
#endif
    if ((r_addr->args & 0x0000ffff) == ((r_addr->args & 0xffff0000) >> 16)) {
        ip6_memaddr *returnID = (ip6_memaddr *) (&target_ip->sin6_addr);
        returnID->cmd = r_addr->cmd;
        returnID->paddr = r_addr->paddr;
        send_udp_raw("", 0, (ip6_memaddr *)&target_ip->sin6_addr, target_ip->sin6_port);
    }
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}


/*
 * TODO: explain.
 * This is freeing target memory?
 */
int free_mem(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr) {
    //print_debug("Content stored at %p has been freed!", (void*)pointer);
    free((void *) *&r_addr->paddr);
    //munmap((void *) pointer, BLOCK_SIZE);
    memcpy(sendBuffer, "ACK", 3);
    ip6_memaddr *returnID = (ip6_memaddr *)&target_ip->sin6_addr;
    returnID->cmd = r_addr->cmd;
    returnID->paddr = r_addr->paddr;
    send_udp_raw(sendBuffer, BLOCK_SIZE, (ip6_memaddr *)&target_ip->sin6_addr, target_ip->sin6_port);
    return EXIT_SUCCESS;
}

void process_request(char *receiveBuffer, struct sockaddr_in6 *targetIP, ip6_memaddr *remoteAddr) {
    // Switch on the client command
    if (remoteAddr->cmd == ALLOC_CMD) {
        print_debug("******ALLOCATE******");
        allocate_mem(targetIP);
    } else if (remoteAddr->cmd == WRITE_CMD) {
        print_debug("******WRITE DATA: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        write_mem(receiveBuffer, targetIP, remoteAddr);
    } else if (remoteAddr->cmd == WRITE_BULK_CMD) {
        print_debug("******WRITE DATA BULK: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        write_mem_bulk(receiveBuffer, targetIP, remoteAddr);
    } else if (remoteAddr->cmd == READ_CMD) {
        print_debug("******READ DATA: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        read_mem(targetIP, remoteAddr);
    } else if (remoteAddr->cmd == READ_BULK_CMD) {
        print_debug("******READ DATA PTR: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        read_mem_ptr(receiveBuffer, targetIP, remoteAddr);
    } else if (remoteAddr->cmd == FREE_CMD) {
        print_debug("******FREE DATA: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        free_mem(targetIP, remoteAddr);
    } else if (remoteAddr->cmd == ALLOC_BULK_CMD) {
        print_debug("******ALLOCATE BULK DATA: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr,IPV6_SIZE);
        uint64_t *alloc_size = (uint64_t *) receiveBuffer;
        allocate_mem_bulk(targetIP, *alloc_size);
    } else {
        printf("Cannot match command %d!\n",remoteAddr->cmd);
    }
}