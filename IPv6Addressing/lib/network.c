#define _GNU_SOURCE

#include "network.h"
#include "utils.h"
#include "udpcooked.h"


#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <unistd.h>           // close()

uint64_t sendLat = 0;
uint64_t send_calls = 0;

uint64_t rcvLat = 0;
uint64_t rcv_calls = 0;


/*
 * Sends message to specified socket
 * RAW version, we craft our own packet.
 */
int sendUDPRaw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP) {
    int dst_port = ntohs(targetIP->sin6_port);
    //char dst_ip[INET6_ADDRSTRLEN];
    //inet_ntop(targetIP->sin6_family,&targetIP->sin6_addr, dst_ip, sizeof dst_ip);
    //print_debug("Sending to %s:%d", dst_ip,dst_port);
    cooked_send(targetIP, dst_port, sendBuffer, msgBlockSize);
    memset(sendBuffer, 0, msgBlockSize);
    return EXIT_SUCCESS;
}

/*
 * Sends message to specified socket
 * RAW version, we craft our own packet.
 */
// TODO: Evaluate what variables and structures are actually needed here
// TODO: Error handling
int sendUDPIPv6Raw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr remotePointer) {
    
    memcpy(&(targetIP->sin6_addr), &remotePointer, sizeof(remotePointer));
    //char dst_ip[INET6_ADDRSTRLEN];
    int dst_port = ntohs(targetIP->sin6_port);
    //inet_ntop(p->ai_family,get_in_addr(p), dst_ip, sizeof dst_ip);
    //print_debug("Sending to %s:%d", dst_ip,dst_port);
    uint64_t start = getns(); 
    cooked_send(targetIP, dst_port, sendBuffer, msgBlockSize);
    sendLat += getns() - start;
    send_calls++;
    memset(sendBuffer, 0, msgBlockSize);
    return EXIT_SUCCESS;
}

/*
 * Receives message on socket
 * RAW version, we craft our own packet.
 */
// TODO: Evaluate what variables and structures are actually needed here
// TODO: Error handling
int receiveUDPIPv6Raw(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr *ipv6Pointer) {
    uint64_t start = getns(); 
    int numbytes = epoll_rcv(receiveBuffer, msgBlockSize, targetIP, ipv6Pointer);
//     int numbytes = cooked_receive(receiveBuffer, msgBlockSize, targetIP, ipv6Pointer);

    rcvLat += getns() - start;
    rcv_calls++;
    return numbytes;
}

/*
 * Get a POINTER_SIZE pointer from an IPV6_SIZE ip address 
 */
uint64_t getPointerFromIPv6(struct in6_addr addr) {
    uint64_t pointer = 0;
    memcpy(&pointer,addr.s6_addr+IPV6_SIZE-POINTER_SIZE, POINTER_SIZE);
    // printf("Converted IPv6 to Pointer: ");
    // printNBytes((char*) &pointer,POINTER_SIZE);
    return pointer;
}

/*
 * Convert a POINTER_SIZE bit pointer to a IPV6_SIZE bit IPv6 address\
 * (beginning at the POINTER_SIZEth bit)
 */
struct in6_addr getIPv6FromPointer(uint64_t pointer) {
    struct in6_addr * newAddr = (struct in6_addr *) calloc(1, sizeof(struct in6_addr));
    char s[INET6_ADDRSTRLEN];
    // printf("Memcpy in getIPv6FromPointer\n");
    memcpy(newAddr->s6_addr+IPV6_SIZE-POINTER_SIZE, (char *)pointer, POINTER_SIZE);
    memcpy(newAddr->s6_addr+4,&SUBNET_ID,1);
    inet_ntop(AF_INET6, newAddr, s, sizeof s);
    print_debug("IPv6 Pointer %s",s);
    return *newAddr;
}

void printSendLat() {
    printf("Average Sending Time %lu us\n", (sendLat/1000)/send_calls );
    printf("Average Receive Time %lu us\n", (rcvLat/1000)/rcv_calls );

}