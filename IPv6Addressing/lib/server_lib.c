#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <string.h>           // strcpy, memset(), and memcpy()

#include "utils.h"
#include "network.h"
#include "config.h"

static __thread char sendBuffer[BLOCK_SIZE];




/*
 * Frees global memory
 */
// DEPRECATED 
char *varadr_char[1000];
int countchar = 0;
int cleanMemory() {
    int i;
    for (i = 0; i <= countchar; i++) {
        free(varadr_char[i]);
    }

    return EXIT_SUCCESS;
}

/*
 * Adds a character to the global memory
 */
// DEPRECATED 
int addchar(char* charadr) {
    if (charadr == NULL ) {
        perror("\nError when trying to allocate a pointer! \n");
        cleanMemory();
        exit(EXIT_FAILURE);
    }
    varadr_char[countchar] = charadr;
    countchar++;
    return EXIT_SUCCESS;
}

/*
 * Get a POINTER_SIZE pointer from an IPV6_SIZE ip address 
 */
// DEPRECATED 
uint64_t getPointerFromIPv6(struct in6_addr addr) {
    uint64_t pointer;
    memcpy(&pointer,addr.s6_addr+IPV6_SIZE-POINTER_SIZE, POINTER_SIZE);
    return pointer;
}

/*
 * Convert a POINTER_SIZE bit pointer to a IPV6_SIZE bit IPv6 address\
 * (beginning at the POINTER_SIZEth bit)
 */
// DEPRECATED 
struct in6_addr getIPv6FromPointer(uint64_t pointer) {
    struct in6_addr *newAddr = (struct in6_addr *) calloc(1, sizeof(struct in6_addr));
    // printf("Memcpy in getIPv6FromPointer\n");
    memcpy(newAddr->s6_addr+IPV6_SIZE-POINTER_SIZE, (char *)pointer, POINTER_SIZE);
    memcpy(newAddr->s6_addr+4,&SUBNET_ID,2);
    return *newAddr;
}

/*
 * TODO: explain.
 * Allocates local memory and exposes it to a client requesting it
 */
struct in6_memaddr allocPointer; // Keep this struct global as we reaccess it many times
int allocateMem(struct sockaddr_in6 *targetIP) {
    //TODO: Error handling if we runt out of memory, this will fail
    //do some work, which might goto error
    //void *allocated = calloc(BLOCK_SIZE, sizeof(char));
    //void *allocated = malloc(BLOCK_SIZE);
    void *allocated = calloc(1 ,BLOCK_SIZE);
    //void *allocated = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    //if (allocated == (void *) MAP_FAILED) perror("mmap"), exit(1);
    memset(&allocPointer, 0, IPV6_SIZE);
    memcpy(&allocPointer.paddr, &allocated, POINTER_SIZE);
    memcpy(&allocPointer.subid, &SUBNET_ID, 2);
    //struct in6_addr ipv6Pointer; = getIPv6FromPointer((uint64_t) &allocated);
    memcpy(sendBuffer, "ACK", 3);
    memcpy(sendBuffer+3, &allocPointer, IPV6_SIZE); 
    send_udp_raw(sendBuffer, BLOCK_SIZE, targetIP);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * Gets memory and sends it
 */
int getMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *ipv6Pointer) {

    //uint64_t *pointer = &ipv6Pointer->paddr;
    //memcpy(&pointer,addr.s6_addr+IPV6_SIZE-POINTER_SIZE, POINTER_SIZE);
    //uint64_t pointer1 = getPointerFromIPv6(*ipv6Pointer);
    //print_debug("Content length %lu is currently stored at %p!", strlen((char *)pointer), (void*)pointer);
    //print_debug("Content preview (50 bytes): %.50s", (char *)pointer);

    // Send the sendBuffer (entire BLOCK_SIZE) to sock_fd
    // print_debug("Content length %lu will be delivered to client!", strlen((char *)pointer));
    struct in6_memaddr *returnID = (struct in6_memaddr *) (&targetIP->sin6_addr);
/*    for (int i =0; i<=BLOCK_SIZE; i++) {
        printf("%c", ((unsigned char *)*(&ipv6Pointer->paddr))[i]);
    }*/
    returnID->cmd = ipv6Pointer->cmd;
    returnID->paddr = ipv6Pointer->paddr;
    send_udp_raw((void *) *&ipv6Pointer->paddr, BLOCK_SIZE, targetIP);

    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int writeMem(char *receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_memaddr *ipv6Pointer) {
    //print_debug("Data received (first 50 bytes): %.50s", dataToWrite);
    //uint64_t *pointer = &ipv6Pointer->paddr;
    //uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);
    // Copy the first POINTER_SIZE bytes of receive buffer into the target
    //uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);
    printf("w");

    memcpy((void *) *(&ipv6Pointer->paddr), receiveBuffer, BLOCK_SIZE); 
    //print_debug("Content length %lu is currently stored at %p!", strlen((char *)pointer), (void*)pointer);
    //print_debug("Content preview (50 bytes): %.50s", (char *)pointer);

    memcpy(sendBuffer, "ACK", 3);
    struct in6_memaddr *returnID = (struct in6_memaddr *) (&targetIP->sin6_addr);
    returnID->cmd = ipv6Pointer->cmd;
    returnID->paddr = ipv6Pointer->paddr;
    send_udp_raw(sendBuffer, BLOCK_SIZE, targetIP);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * This is freeing target memory?
 */
int freeMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *ipv6Pointer) {
    //uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);    
    //uint64_t *pointer = &ipv6Pointer->paddr;
    //print_debug("Content stored at %p has been freed!", (void*)pointer);
    free((void *) *&ipv6Pointer->paddr);
    //munmap((void *) pointer, BLOCK_SIZE);
    memcpy(sendBuffer, "ACK", 3);
    struct in6_memaddr *returnID = (struct in6_memaddr *) (&targetIP->sin6_addr);
    returnID->cmd = ipv6Pointer->cmd;
    returnID->paddr = ipv6Pointer->paddr;
    send_udp_raw(sendBuffer, BLOCK_SIZE, targetIP);
    return EXIT_SUCCESS;
}
