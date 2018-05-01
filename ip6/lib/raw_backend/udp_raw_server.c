#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <errno.h>            // errno, perror()
#include <sys/epoll.h>        // epoll_wait(), epoll_event
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "udp_raw_common.h"
#include "../utils.h"
#include "../config.h"
#include "../server_lib.h"

static __thread int epoll_fd_g = -1;
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
    setup_rx_socket(cfg, my_port, thread_id, &epoll_fd_g, &ring_rx_g);
}

void process_request(char *receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {

    // Switch on the client command
    if (remoteAddr->cmd == ALLOC_CMD) {
        print_debug("******ALLOCATE******");
        allocate_mem(targetIP);
    } else if (remoteAddr->cmd == WRITE_CMD) {
        print_debug("******WRITE DATA: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        write_mem(receiveBuffer, targetIP, remoteAddr);
    } else if (remoteAddr->cmd == GET_CMD) {
        print_debug("******GET DATA: ");
        if (DEBUG) print_n_bytes((char *) remoteAddr, IPV6_SIZE);
        get_mem(targetIP, remoteAddr);
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


int epoll_server_rcv(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    while (1) {
        struct epoll_event events[1024];

        int num_events = epoll_wait(epoll_fd_g, events, sizeof events / sizeof *events, 0);
        //int num_events = epoll_wait(epoll_fd, events, sizeof events / sizeof *events, -1);
        /*if (num_events == 0 && !server) {
            //printf("TIMEOUT!\n");
            return -1;
        }*/

        for (int i = 0; i < num_events; i++)  {
            struct epoll_event *event = &events[i];
            if (event->events & EPOLLIN) {
                struct tpacket_hdr *tpacket_hdr = get_packet(&ring_rx_g);
                if ( tpacket_hdr->tp_status == TP_STATUS_KERNEL) {
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
                struct ethhdr *ethhdr = (struct ethhdr *)((char *) tpacket_hdr + tpacket_hdr->tp_mac);
                struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
                struct udphdr *udphdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
                char *payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
                // This should be debug code...
                /*
                char s[INET6_ADDRSTRLEN];
                char s1[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
                inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
                printf("Thread %d Got message from %s:%d to %s:%d\n", thread_id, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
                printf("Thread %d My port %d their dest port %d\n",thread_id, ntohs(my_port), ntohs(udphdr->dest) );
                */
                memcpy(receiveBuffer, payload, msgBlockSize);
                if (remoteAddr != NULL) {
                    memcpy(remoteAddr, &iphdr->ip6_dst, IPV6_SIZE);
                }
                memcpy(targetIP->sin6_addr.s6_addr, &iphdr->ip6_src, IPV6_SIZE);
                targetIP->sin6_port = udphdr->source;
                tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                next_packet(&ring_rx_g);
                return msgBlockSize;
           }
        }
    }
    return EXIT_SUCCESS;
}

//This function is hacky bullshit, needs a lot of improvement.
int raw_rcv_loop(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {

    struct epoll_event events[1024];
    while (1){
        int num_events = epoll_wait(epoll_fd_g, events, sizeof events / sizeof *events, 0);
        //int num_events = epoll_wait(epoll_fd, events, sizeof events / sizeof *events, -1);
        /*if (num_events == 0 && !server) {
            //printf("TIMEOUT!\n");
            return -1;
        }*/
        for (int i = 0; i < num_events; i++)  {
            struct epoll_event *event = &events[i];
            if (event->events & EPOLLIN) {
                struct tpacket_hdr *tpacket_hdr = get_packet(&ring_rx_g);
                if ( tpacket_hdr->tp_status == TP_STATUS_KERNEL) {
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
                struct eth_hdr *eth_hdr = (struct eth_hdr *)((char *) tpacket_hdr + tpacket_hdr->tp_mac);
                struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((char *)eth_hdr + ETH_HDRLEN);
                struct udphdr *udp_hdr = (struct udphdr *)((char *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN);
                char *payload = ((char *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
                // This should be debug code...
                /*
                char s[INET6_ADDRSTRLEN];
                char s1[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &ip_hdr->ip6_src, s, sizeof s);
                inet_ntop(AF_INET6, &ip_hdr->ip6_dst, s1, sizeof s1);
                printf("Thread %d Got message from %s:%d to %s:%d\n", thread_id, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
                printf("Thread %d My port %d their dest port %d\n",thread_id, ntohs(my_port), ntohs(udphdr->dest) );
                */
                memcpy(receiveBuffer, payload, msgBlockSize);
                if (remoteAddr != NULL) {
                    memcpy(remoteAddr, &ip_hdr->ip6_dst, IPV6_SIZE);
                }
                memcpy(targetIP->sin6_addr.s6_addr, &ip_hdr->ip6_src, IPV6_SIZE);
                targetIP->sin6_port = udp_hdr->source;
                tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                next_packet(&ring_rx_g);
                process_request(payload, targetIP, (struct in6_memaddr *)&ip_hdr->ip6_dst);
           }
        }
    }
}


void enter_raw_server_loop(uint16_t server_port) {
    char receiveBuffer[BLOCK_SIZE];
    struct sockaddr_in6 *targetIP = calloc(1, sizeof(struct sockaddr_in6));
    targetIP->sin6_port = htons(server_port);
    struct in6_memaddr remoteAddr;
    raw_rcv_loop(receiveBuffer, BLOCK_SIZE, targetIP, &remoteAddr);
}

int get_rx_socket_server() {
    return sd_rx_g;
}

void close_rx_socket_server() {
    close(sd_rx_g);
    sd_rx_g = -1;
    close_epoll(epoll_fd_g, ring_rx_g);
}

