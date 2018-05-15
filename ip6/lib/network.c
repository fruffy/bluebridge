#define _GNU_SOURCE
#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <unistd.h>           // close()
#include <pthread.h>          // close()
#include "network.h"
#include "utils.h"
#include "raw_backend/udp_raw_common.h"
#ifndef DEFAULT
#include "dpdk_backend/dpdk_common.h"
#endif

static __thread uint64_t sendLat = 0;
static __thread uint64_t send_calls = 0;

static __thread uint64_t rcv_lat = 0;
static __thread uint64_t rcv_calls = 0;


/*
 * Sends message to specified socket
 * RAW version, we craft our own packet.
 */
// TODO: Evaluate what variables and structures are actually needed here
// TODO: Error handling
int send_udp_raw(char *tx_buf, int msg_size, ip6_memaddr *remote_addr, int dst_port) {
    uint64_t start = getns(); 
    pkt_rqst pkt = {
        .dst_addr = *remote_addr,
        .dst_port = dst_port,
        .data = tx_buf,
        .datalen = msg_size
    };
#ifdef DEFAULT
    cooked_send(pkt);
#else
    dpdk_send(pkt);
#endif
    sendLat += getns() - start;
    send_calls++;
    //memset(tx_buf, 0, msg_size);
    return EXIT_SUCCESS;
}

/*
 * Sends message to specified socket
 * RAW version, we craft our own packet.
 */
// TODO: Evaluate what variables and structures are actually needed here
// TODO: Error handling
int send_udp_raw_batched(pkt_rqst *pkts, uint32_t *sub_ids, int num_addrs) {
    uint64_t start = getns();
#ifdef DEFAULT
    cooked_batched_send(pkts, num_addrs, sub_ids);
#else
    dpdk_send(pkt);
#endif
    sendLat += getns() - start;
    send_calls++;
    //memset(tx_buf, 0, msg_size);
    return EXIT_SUCCESS;
}


/*
 * Receives message on socket
 * RAW version, we craft our own packet.
 */
// TODO: Error handling
int rcv_udp6_raw(char *rx_buf, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
    uint64_t start = getns();
    int numbytes;
#ifdef DEFAULT
    numbytes = simple_epoll_rcv(rx_buf, target_ip, remote_addr);
#else
    numbytes = dpdk_client_rcv(rx_buf, msg_size, target_ip, remote_addr);
#endif
    rcv_lat += getns() - start;
    rcv_calls++;
    return numbytes;
}

/*
 * Receives message on socket
 * RAW version, we craft our own packet.
 */
// TODO: Error handling
void rcv_udp6_raw_bulk(int num_packets) {
    uint64_t start = getns();
#ifdef DEFAULT
    write_packets(num_packets);
#else
#endif
    rcv_lat += getns() - start;
    rcv_calls++;
}


struct sockaddr_in6 *init_sockets(struct config *bb_conf, int server) {
#ifdef DEFAULT
    (void) server;
    init_simple_rx_socket(bb_conf);
    init_tx_socket(bb_conf);
#else
    if (server)
        init_server_dpdk(bb_conf);
    else 
        init_client_dpdk(bb_conf);
#endif
    struct sockaddr_in6 *temp = malloc(sizeof(struct sockaddr_in6));
    temp->sin6_port = htons(strtol(bb_conf->server_port, (char **)NULL, 10));
    return temp;
}

void launch_server_loop(struct config *bb_conf) {
    (void) bb_conf; // Placeholder
#ifdef DEFAULT
    init_tx_socket(bb_conf);
    init_rx_socket_server(bb_conf);
    enter_raw_server_loop(bb_conf->src_port);
#else
    init_server_dpdk(bb_conf);
    enter_dpdk_server_loop(bb_conf->src_port);
#endif
}

void close_sockets(int server) {
    close_tx_socket();
    if (server)
        close_rx_socket_server();
    else
        close_rx_socket_client();
}

void set_net_thread_ids(int t_id) {
    set_thread_id_tx(t_id);
    set_thread_id_rx_client(t_id);
    set_thread_id_rx_server(t_id);
}

struct sockaddr_in6 *init_net_thread(int t_id, struct config *bb_conf, int isServer) {
    if (!isServer)
        set_net_thread_ids(t_id);
    return init_sockets(bb_conf, 0);
}

void printSendLat() {
    if (send_calls == 0 || rcv_calls == 0)
        return;
    printf("Average Sending Time: "KRED"%lu"RESET" ns "KRED"%.2f"RESET" us\n",
     sendLat/send_calls, (double) sendLat/(send_calls*1000)  );
    printf("Average Receive Time: "KRED"%lu"RESET" ns "KRED"%.2f"RESET" us\n",
     rcv_lat/rcv_calls, (double)  rcv_lat/(rcv_calls*1000) );
}
