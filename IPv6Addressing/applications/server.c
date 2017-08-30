/*
 ** server.c -- a stream socket server demo
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../lib/utils.h"
#include "../lib/network.h"
#define BACKLOG 10     // how many pending connections queue will hold

char *varadr_char[1000];
int countchar = 0;

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
int providePointer(struct sockaddr_in6 *targetIP) {
    //TODO: Error handling if we runt out of memory, this will fail
    char * allocated = (char *) calloc(BLOCK_SIZE,sizeof(char));
    char sendBuffer[BLOCK_SIZE];
    //void *allocated = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    //if (allocated == (void *) MAP_FAILED) perror("mmap"), exit(1);
    int size = 0;
    print_debug("Input pointer: %p", (void *) allocated);
    struct in6_addr  ipv6Pointer = getIPv6FromPointer((uint64_t) &allocated);
    memcpy(sendBuffer, "ACK:", sizeof("ACK:"));
    size += sizeof("ACK:") - 1;
    memcpy(sendBuffer+size, &ipv6Pointer, IPV6_SIZE); 
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);


/*  print_debug("Allocated Pointer: %p -> %s",allocated, allocated);
*/
    // TODO change to be meaningful, i.e., error message
    return 0;
}

int sendAddr(struct sockaddr_in6 *targetIP, char* receiveBuffer) {
    //TODO: Error handling if we runt out of memory, this will fail
    char sendBuffer[BLOCK_SIZE];
    int size = 0;
    uint64_t pointer = 0;
    memcpy(&pointer, receiveBuffer, POINTER_SIZE);
    print_debug("Input pointer: %p", (void *) pointer);
    struct in6_addr ipv6Pointer = getIPv6FromPointer((uint64_t) &pointer);
    memcpy(sendBuffer, "ACK:", sizeof("ACK:"));
    size += sizeof("ACK:") - 1;
    memcpy(sendBuffer+size, &ipv6Pointer, IPV6_SIZE); 
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);

    // TODO change to be meaningful, i.e., error message
    return 0;
}

/*
 * TODO: explain.
 * Writes a piece of memory?
 */
int writeMem(char * receiveBuffer, struct sockaddr_in6 * targetIP, struct in6_addr * ipv6Pointer) {
    char sendBuffer[BLOCK_SIZE];    //Point to data after command
    char * dataToWrite = receiveBuffer + 1;

    print_debug("Data received (first 50 bytes): %.50s", dataToWrite);

    uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);
    // Copy the first POINTER_SIZE bytes of receive buffer into the target
    print_debug("Target pointer: %p", (void *) pointer);

    memcpy((void *) pointer, dataToWrite, BLOCK_SIZE); 
    
    print_debug("Content length %lu is currently stored at %p!", strlen((char *)pointer), (void*)pointer);
    print_debug("Content preview (50 bytes): %.50s", (char *)pointer);
    
    memcpy(sendBuffer, "ACK", 3);
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);

    // TODO change to be meaningful, i.e., error message
    return 0;
}

/*
 * TODO: explain.
 * This is freeing target memory?
 */
int freeMem(struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer) {
    char sendBuffer[BLOCK_SIZE];
    uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);    
    
    print_debug("Content stored at %p has been freed!", (void*)pointer);
    
    free((void *) pointer);
  //munmap((void *) pointer, BLOCK_SIZE);
    memcpy(sendBuffer, "ACK", sizeof("ACK"));
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);
    // TODO change to be meaningful, i.e., error message
    return 0;
}

/*
 * Gets memory and sends it
 */
int getMem(struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer) {
    char * sendBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
    uint64_t pointer = getPointerFromIPv6(*ipv6Pointer);

    // print_debug("Content length %lu is currently stored at %p!", strlen((char *)pointer), (void*)pointer);
    // print_debug("Content preview (50 bytes): %.50s", (char *)pointer);

    memcpy(sendBuffer, (void *) pointer, BLOCK_SIZE);
    // Send the sendBuffer (entire BLOCK_SIZE) to sock_fd
    // print_debug("Content length %lu will be delivered to client!", strlen((char *)pointer));
    //sendUDP(sock_fd, sendBuffer, BLOCK_SIZE, p);
    sendUDPRaw(sendBuffer, BLOCK_SIZE, targetIP);

    free(sendBuffer);
    // TODO change to be meaningful, i.e., error message
    return 0;
}

/*
 * Request handler for socket sock_fd
 * TODO: get message format
 */
void handleClientRequests(char * receiveBuffer, struct in6_addr * ipv6Pointer, struct sockaddr_in6 *targetIP) {
    char * splitResponse;
    // Switch on the client command
    if (memcmp(receiveBuffer, ALLOC_CMD,2) == 0) {
        print_debug("******ALLOCATE******");
        splitResponse = receiveBuffer+2;
        providePointer(targetIP);
    } else if (memcmp(receiveBuffer, WRITE_CMD,2) == 0) {
        splitResponse = receiveBuffer+2;
        print_debug("******WRITE DATA: ");
        if (DEBUG) {
            printNBytes((char *) ipv6Pointer, IPV6_SIZE);
        }
        writeMem(splitResponse, targetIP, ipv6Pointer);
    } else if (memcmp(receiveBuffer, GET_CMD,2) == 0) {
        splitResponse = receiveBuffer+2;
        print_debug("******GET DATA: ");
        // printNBytes((char *) ipv6Pointer,IPV6_SIZE);
        getMem(targetIP, ipv6Pointer);
    } else if (memcmp(receiveBuffer, FREE_CMD,2) == 0) {
        splitResponse = receiveBuffer+2;
        print_debug("******FREE DATA: ");
        if (DEBUG) {
            printNBytes((char *) ipv6Pointer,IPV6_SIZE);
        }
        freeMem(targetIP, ipv6Pointer);
    } else if (memcmp(receiveBuffer, GET_ADDR_CMD,2) == 0) {
        splitResponse = receiveBuffer+2;
        print_debug("******GET ADDRESS: ");
        if (DEBUG) {
            printf("Calling send address\n");
            // printNBytes((char *) ipv6Pointer,IPV6_SIZE);
            printNBytes(splitResponse, POINTER_SIZE);
        }
        sendAddr(targetIP, splitResponse);
    } else {
        printf("Cannot match command!\n");
/*      if (sendUDP(sock_fd, "Hello, world!", 13, p) == -1) {
            perror("ERROR writing to socket");
            exit(1);
        }*/
        if (sendUDPRaw("Hello, world!", 13, targetIP) == -1) {
            perror("ERROR writing to socket");
            exit(1);
        }
    }
}

/*
 * Main workhorse method. Parses command args and does setup.
 * Blocks waiting for connections.
 */
int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Defaulting to standard values...\n");
        argv[1] = "5000";
    }
    genPacketInfo(argv[1]);
    struct sockaddr_in6 *temp = init_rcv_socket();
//    struct sockaddr_in6 *temp = init_rcv_socket_old(argv[1]);
    init_send_socket();
    // Start waiting for connections
    while (1) {
        char * receiveBuffer = (char *) calloc(BLOCK_SIZE,sizeof(char));
        struct in6_addr * ipv6Pointer = (struct in6_addr *) calloc(1,sizeof(struct in6_addr));
        //TODO: Error handling (numbytes = -1)
        receiveUDPIPv6Raw(receiveBuffer, BLOCK_SIZE, temp, ipv6Pointer);
        handleClientRequests(receiveBuffer, ipv6Pointer, temp);
    }
    close_sockets();
    return 0;
}
