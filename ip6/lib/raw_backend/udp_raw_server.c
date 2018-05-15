#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <errno.h>            // errno, perror()
#include <sys/epoll.h>        // epoll_wait(), epoll_event
#include <sys/poll.h>         // pollfd, poll
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "udp_raw_common.h"
#include "../utils.h"
#include "../config.h"
#include "../server_lib.h"

static __thread int epoll_fd_g = -1;
//static __thread struct pollfd poll_fd_g;
static __thread struct rx_ring ring_rx_g;
static __thread int sd_rx_g;
static __thread int thread_id;

static __thread uint16_t my_port = 0;
static __thread char my_addr[INET6_ADDRSTRLEN];


void set_thread_id_rx_server(int id) {
    thread_id = id;
}

/* Initialize a listening socket
   epoll_fd and ring_rx geht filled by the function */
void init_rx_socket_server(struct config *cfg) {
    inet_ntop(AF_INET6, &cfg->src_addr, my_addr, INET6_ADDRSTRLEN);
    my_port = cfg->src_port;
    setup_rx_socket(cfg, my_port, thread_id, &epoll_fd_g, NULL, &ring_rx_g);
}

void handle_packet(struct tpacket_hdr *tpacket_hdr, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
    struct eth_hdr *eth_hdr = (struct eth_hdr *)((char *) tpacket_hdr + tpacket_hdr->tp_mac);
    struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((char *)eth_hdr + ETH_HDRLEN);
    struct udphdr *udp_hdr = (struct udphdr *)((char *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN);
    char *payload = ((char *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
    // This should be debug code...
    // printf("Thread %d Message from ", thread_id), print_ip_addr(&ip_hdr->ip6_src);
    // printf("_%d to ", ntohs(udp_hdr->source)), print_ip_addr(&ip_hdr->ip6_dst), printf("_%d\n", ntohs(udp_hdr->dest));
    uint16_t msg_size = htons(udp_hdr->len) - UDP_HDRLEN;
    if (remote_addr != NULL)
        memcpy(remote_addr, &ip_hdr->ip6_dst, IPV6_SIZE);
    memcpy(target_ip->sin6_addr.s6_addr, &ip_hdr->ip6_src, IPV6_SIZE);
    target_ip->sin6_port = udp_hdr->source;
    tpacket_hdr->tp_status = TP_STATUS_KERNEL;
    next_packet(&ring_rx_g);
    process_request(target_ip, (ip6_memaddr *)&ip_hdr->ip6_dst, payload, msg_size);
}

//This function is hacky bullshit, needs a lot of improvement.
int raw_rcv_loop(struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
    struct epoll_event event;
    while (1){
        epoll_wait(epoll_fd_g, &event, sizeof event, -1);
            if (event.events & EPOLLIN) {
                struct tpacket_hdr *tpacket_hdr = get_packet(&ring_rx_g);
                if ((volatile uint32_t)tpacket_hdr->tp_status == TP_STATUS_KERNEL) {
                    //printf("Dis Kernel\n");
                    continue;
                }
                if (tpacket_hdr->tp_status & TP_STATUS_COPY) {
                    next_packet(&ring_rx_g);
                    continue;
                }
                if (tpacket_hdr->tp_status & TP_STATUS_LOSING) {
                    next_packet(&ring_rx_g);
                    continue;
                }
                handle_packet(tpacket_hdr, target_ip, remote_addr);
           }
    }
}

void enter_raw_server_loop(uint16_t server_port) {
    struct sockaddr_in6 *target_ip = calloc(1, sizeof(struct sockaddr_in6));
    target_ip->sin6_port = htons(server_port);
    ip6_memaddr remote_addr;
    raw_rcv_loop(target_ip, &remote_addr);
}

int get_rx_socket_server() {
    return sd_rx_g;
}

void close_rx_socket_server() {
    close(sd_rx_g);
    sd_rx_g = -1;
    close_epoll(epoll_fd_g, ring_rx_g);
}

