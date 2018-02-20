#define _GNU_SOURCE

#include "network.h"
#include "utils.h"
#include "udpcooked.h"
#ifndef DEFAULT
#include "dpdk_common.h"
#endif
#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <unistd.h>           // close()
#include <pthread.h>           // close()

static __thread uint64_t sendLat = 0;
static __thread uint64_t send_calls = 0;

static __thread uint64_t rcvLat = 0;
static __thread uint64_t rcv_calls = 0;


/*
 * Sends message to specified socket
 * RAW version, we craft our own packet.
 */
int send_udp_raw(char *sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP) {
    //char dst_ip[INET6_ADDRSTRLEN];
    //inet_ntop(AF_INET6,&targetIP->sin6_addr, dst_ip, sizeof dst_ip);
    //print_debug("Sending to %s:%d", dst_ip,dst_port);
    struct pkt_rqst pkt = {
        .dst_addr = &targetIP->sin6_addr,
        .dst_port = targetIP->sin6_port,
        .data = sendBuffer,
        .datalen = msgBlockSize
    };
#ifdef DEFAULT
    cooked_send(pkt);
#else
    dpdk_send(pkt);
#endif
    //memset(sendBuffer, 0, msgBlockSize);
    return EXIT_SUCCESS;
}

/*
 * Sends message to specified socket
 * RAW version, we craft our own packet.
 */
// TODO: Evaluate what variables and structures are actually needed here
// TODO: Error handling
int send_udp6_raw(char *sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    uint64_t start = getns(); 
    struct pkt_rqst pkt = {
        .dst_addr = (struct in6_addr *) remoteAddr,
        .dst_port = targetIP->sin6_port,
        .data = sendBuffer,
        .datalen = msgBlockSize
    };
#ifdef DEFAULT
    cooked_send(pkt);
#else
    dpdk_send(pkt);
#endif
    sendLat += getns() - start;
    send_calls++;
    //memset(sendBuffer, 0, msgBlockSize);
    return EXIT_SUCCESS;
}

/*
 * Receives message on socket
 * RAW version, we craft our own packet.
 */
// TODO: Error handling
int rcv_udp6_raw(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    uint64_t start = getns();
    int numbytes;
#ifdef DEFAULT
    numbytes = epoll_rcv(receiveBuffer, msgBlockSize, targetIP, remoteAddr, 1);
#else
    numbytes = dpdk_server_rcv(receiveBuffer, msgBlockSize, targetIP, remoteAddr, 1);
#endif
    rcvLat += getns() - start;
    rcv_calls++;
    return numbytes;
}

/*
 * Receives message on socket
 * RAW version, we craft our own packet.
 */
// TODO: Error handling
int rcv_udp6_raw_id(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    uint64_t start = getns();
    int numbytes;
#ifdef DEFAULT
    numbytes = epoll_rcv(receiveBuffer, msgBlockSize, targetIP, remoteAddr, 0);
#else
    numbytes = dpdk_client_rcv(receiveBuffer, msgBlockSize, targetIP, remoteAddr, 0);
#endif
    rcvLat += getns() - start;
    rcv_calls++;
    return numbytes;
}

struct sockaddr_in6 *init_sockets(struct config *bb_conf, int server) {
#ifdef DEFAULT
    if (server) {} // Currently a null operation until we have a raw socket server
    struct sockaddr_in6 * temp = init_rcv_socket(bb_conf);
    init_send_socket(bb_conf);
#else
    if (server)
        init_server_dpdk(bb_conf);
    else 
        init_client_dpdk(bb_conf);
    struct sockaddr_in6 *temp = malloc(sizeof(struct sockaddr_in6));
#endif
    temp->sin6_port = htons(strtol(bb_conf->server_port, (char **)NULL, 10));
    return temp;
}

void launch_server_loop(struct config *bb_conf) {
    (void) bb_conf; // Placeholder
#ifndef DEFAULT
    init_server_dpdk(bb_conf);
    enter_server_loop(bb_conf->src_port);
#endif
}




void close_sockets() {
    close_send_socket();
    close_rcv_socket();
}

void set_net_thread_ids(int t_id) {
    set_thread_id_tx(t_id);
    set_thread_id_rx(t_id);
}

struct sockaddr_in6 *init_net_thread(int t_id, struct config *bb_conf, int isServer) {
    if (!isServer)
        set_net_thread_ids(t_id);
    return init_sockets(bb_conf, 0);
}

void printSendLat() {
    if (send_calls == 0 || rcv_calls == 0)
        return;
    printf("Average Sending Time %lu ns\n", (sendLat)/send_calls );
    printf("Average Receive Time %lu ns\n", (rcvLat)/rcv_calls );

}
