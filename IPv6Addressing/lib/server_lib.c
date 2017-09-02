#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "network.h"
#include "config.h"


char *varadr_char[1000];
int countchar = 0;
static char sendBuffer[BLOCK_SIZE];
/*
 * Frees global memory
 */
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
 * TODO: explain.
 * Allocates local memory and exposes it to a client requesting it
 */
int allocateMem(struct sockaddr_in6 *targetIP) {
    //TODO: Error handling if we runt out of memory, this will fail
    char * allocated = (char *) malloc(BLOCK_SIZE);
    //void *allocated = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    //if (allocated == (void *) MAP_FAILED) perror("mmap"), exit(1);
    int size = sizeof("ACK");
    print_debug("Input pointer: %p", (void *) allocated);
    struct in6_addr  ipv6Pointer = getIPv6FromPointer((uint64_t) &allocated);
    memcpy(sendBuffer, "ACK", size);
    memcpy(sendBuffer+size, &ipv6Pointer, IPV6_SIZE); 
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);
    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * Gets memory and sends it
 */
int getMem(struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer) {
    uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);

    // print_debug("Content length %lu is currently stored at %p!", strlen((char *)pointer), (void*)pointer);
    // print_debug("Content preview (50 bytes): %.50s", (char *)pointer);

    // Send the sendBuffer (entire BLOCK_SIZE) to sock_fd
    // print_debug("Content length %lu will be delivered to client!", strlen((char *)pointer));
    sendUDPRaw((void *) pointer, BLOCK_SIZE, targetIP);

    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int writeMem(char *receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_addr *ipv6Pointer) {
    char *dataToWrite = receiveBuffer + 1;

    //print_debug("Data received (first 50 bytes): %.50s", dataToWrite);

    uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);
    // Copy the first POINTER_SIZE bytes of receive buffer into the target
    //print_debug("Target pointer: %p", (void *) pointer);

    memcpy((void *) pointer, dataToWrite, BLOCK_SIZE); 
    //print_debug("Content length %lu is currently stored at %p!", strlen((char *)pointer), (void*)pointer);
    //print_debug("Content preview (50 bytes): %.50s", (char *)pointer);

    memcpy(sendBuffer, "ACK", 3);
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);

    // TODO change to be meaningful, i.e., error message
    return EXIT_SUCCESS;
}

/*
 * TODO: explain.
 * This is freeing target memory?
 */
int freeMem(struct sockaddr_in6 *targetIP, struct in6_addr *ipv6Pointer) {
    uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);    
    
    print_debug("Content stored at %p has been freed!", (void*)pointer);
    
    free((void *) pointer);
    //munmap((void *) pointer, BLOCK_SIZE);
    memcpy(sendBuffer, "ACK", sizeof("ACK"));
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);
    return EXIT_SUCCESS;
}
