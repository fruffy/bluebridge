#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netinet/udp.h>      // struct udphdr
#include <errno.h>            // errno, perror()
#include <sys/epoll.h>        // epoll_wait(), epoll_event
#include <arpa/inet.h>        // inet_pton() and inet_ntop()


#include "udp_raw_common.h"
#include "../utils.h"
#include "../config.h"

static __thread int epoll_fd_g = -1;
static __thread struct rx_ring ring_rx_g;
static __thread int sd_rx_g;
static __thread int thread_id;
const int TIMEOUT = 0;

static __thread uint16_t my_port = 0;
static __thread char my_addr[INET6_ADDRSTRLEN];


void set_thread_id_rx_client(int id) {
    thread_id = id;
}

/* Initialize a listening socket
   epoll_fd and ring_rx geht filled by the function */
void init_rx_socket_client(struct config *cfg) {
    inet_ntop(AF_INET6, &cfg->src_addr, my_addr, INET6_ADDRSTRLEN);
    my_port = cfg->src_port;
    setup_rx_socket(cfg, my_port, thread_id, &epoll_fd_g, &ring_rx_g);
}

int epoll_client_rcv(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    struct epoll_event events[1024];
    while (1) {

        int num_events = epoll_wait(epoll_fd_g, events, sizeof events / sizeof *events, 0);
        //int num_events = epoll_wait(epoll_fd, events, sizeof events / sizeof *events, -1);
        /*if (num_events == 0 && !server) {
            //printf("TIMEOUT!\n");
            return -1;
        }*/
        for (int i = 0; i < num_events; i++)  {
            struct epoll_event *event = &events[i];
            if (event->events & EPOLLIN) {
                volatile struct tpacket_hdr *tpacket_hdr = get_packet(&ring_rx_g);
                if ( tpacket_hdr->tp_status == TP_STATUS_KERNEL) {
                    next_packet(&ring_rx_g);
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
                
                // char s[INET6_ADDRSTRLEN];
                // char s1[INET6_ADDRSTRLEN];
                // inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
                // inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
                // printf("Thread %d Got message from %s:%d to %s:%d\n", thread_id, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
                // printf("Thread %d My port %d their dest port %d\n",thread_id, ntohs(my_port), ntohs(udphdr->dest) );
                struct in6_memaddr *inAddress =  (struct in6_memaddr *) &iphdr->ip6_dst;
                int isMyID = 1;
                // Terrible hacks inbound, this code needs to be burned.

                if (remoteAddr != NULL) {
                    //printf("Thread %d Their ID\n", thread_id);
                    //printNBytes(inAddress, 16);
                    //printf("Thread %d MY ID\n", thread_id);
                    //printNBytes(remoteAddr, 16)
                    if (inAddress->cmd  == 01 || inAddress->cmd  == 06 )
                        isMyID = 1;
                    else
                        isMyID = (inAddress->cmd == remoteAddr->cmd) && (inAddress->paddr == remoteAddr->paddr);
                }
                if (isMyID) {
                    memcpy(receiveBuffer, payload, msgBlockSize);
                    if (remoteAddr != NULL) {
                        if (remoteAddr->cmd  == 01 || remoteAddr->cmd  == 06)
                            memcpy(remoteAddr, &iphdr->ip6_dst, IPV6_SIZE);
                    }
                    memcpy(targetIP->sin6_addr.s6_addr, &iphdr->ip6_src, IPV6_SIZE);
                    targetIP->sin6_port = udphdr->source;
                    tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                    next_packet(&ring_rx_g);
                    return msgBlockSize;
                }
                tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                next_packet(&ring_rx_g);
           }
        }
    }
    return EXIT_SUCCESS;
}

int get_rx_socket_client() {
    return sd_rx_g;
}

void close_rx_socket_client() {
    close(sd_rx_g);
    sd_rx_g = -1;
    close_epoll(epoll_fd_g, ring_rx_g);
}