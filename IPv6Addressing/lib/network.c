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
int sendUDPRaw(char *sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP) {
    //char dst_ip[INET6_ADDRSTRLEN];
    //inet_ntop(AF_INET6,&targetIP->sin6_addr, dst_ip, sizeof dst_ip);
    //print_debug("Sending to %s:%d", dst_ip,dst_port);
    cooked_send(&targetIP->sin6_addr, targetIP->sin6_port, sendBuffer, msgBlockSize);
    memset(sendBuffer, 0, msgBlockSize);
    return EXIT_SUCCESS;
}

/*
 * Sends message to specified socket
 * RAW version, we craft our own packet.
 */
// TODO: Evaluate what variables and structures are actually needed here
// TODO: Error handling
int sendUDPIPv6Raw(char *sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr remoteAddr) {
    //memcpy(&(targetIP->sin6_addr), &remoteAddr, IPV6_SIZE);
    //char dst_ip[INET6_ADDRSTRLEN];
    //inet_ntop(AF_INET6,&targetIP->sin6_addr, dst_ip, sizeof dst_ip);
    //print_debug("Sending to %s:%d", dst_ip,dst_port);
    uint64_t start = getns(); 
    cooked_send((struct in6_addr *) &remoteAddr, targetIP->sin6_port, sendBuffer, msgBlockSize);
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
int receiveUDPIPv6Raw(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    uint64_t start = getns(); 
    int numbytes = epoll_rcv(receiveBuffer, msgBlockSize, targetIP, remoteAddr);
//     int numbytes = cooked_receive(receiveBuffer, msgBlockSize, targetIP, ipv6Pointer);

    rcvLat += getns() - start;
    rcv_calls++;
    return numbytes;
}

void printSendLat() {
    printf("Average Sending Time %lu ns\n", (sendLat)/send_calls );
    printf("Average Receive Time %lu ns\n", (rcvLat)/rcv_calls );

}