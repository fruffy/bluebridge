#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netinet/udp.h>      // struct udphdr
#include <errno.h>            // errno, perror()
#include <sys/epoll.h>        // epoll_wait(), epoll_event
#include <sys/poll.h>         // pollfd, poll
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
void init_simple_rx_socket(struct config *cfg) {
    inet_ntop(AF_INET6, &cfg->src_addr, my_addr, INET6_ADDRSTRLEN);
    my_port = cfg->src_port;
    setup_rx_socket(cfg, my_port, thread_id, &epoll_fd_g, NULL, &ring_rx_g);
}

//This function is hacky bullshit, needs a lot of improvement.
void write_packets(int num_packets) {
    struct epoll_event event;
    int packets = 0;
    while (1){
        if (packets == num_packets)
            break;
        epoll_wait(epoll_fd_g, &event, sizeof event, 0);
        //int num_events = epoll_wait(epoll_fd, events, sizeof events / sizeof *events, -1);
        /*if (num_events == 0 && !server) {
            //printf("TIMEOUT!\n");
            return -1;
        }*/
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
            struct ethhdr *eth_hdr = (struct ethhdr *)((uint8_t *) tpacket_hdr + tpacket_hdr->tp_mac);
            struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((uint8_t *)eth_hdr + ETH_HDRLEN);
            struct udphdr *udp_hdr = (struct udphdr *)((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN);
            uint8_t *payload = ((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
            ip6_memaddr *inAddress =  (ip6_memaddr *) &ip_hdr->ip6_dst;
            uint16_t msg_size = ntohs(udp_hdr->len) - UDP_HDRLEN;
            memcpy((void *) (inAddress->paddr), payload, msg_size);
            tpacket_hdr->tp_status = TP_STATUS_KERNEL;
            next_packet(&ring_rx_g);
            packets++;
        }
    }
}

int simple_epoll_rcv(uint8_t *rcv_buf, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
    struct epoll_event events[1024];
    while (1) {
        int num_events = epoll_wait(epoll_fd_g, events, sizeof events / sizeof *events, 0);
        //int num_events = epoll_wait(epoll_fd_g, events, sizeof events / sizeof *events, -1);
        /*if (num_events == 0 && !server) {
            //printf("TIMEOUT!\n");
            return -1;
        }*/
        for (int i = 0; i < num_events; i++)  {
            struct epoll_event *event = &events[i];
            if (event->events & EPOLLIN) {
                struct tpacket_hdr *tpacket_hdr = get_packet(&ring_rx_g);
                // Why volatile? See here:
                // https://stackoverflow.com/questions/16359158/some-issues-with-packet-mmap
                if ((volatile uint32_t)tpacket_hdr->tp_status == TP_STATUS_KERNEL) {
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
                struct ethhdr *eth_hdr = (struct ethhdr *)((uint8_t *) tpacket_hdr + tpacket_hdr->tp_mac);
                struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((uint8_t *)eth_hdr + ETH_HDRLEN);
                struct udphdr *udp_hdr = (struct udphdr *)((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN);
                uint8_t *payload = ((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
                // This should be debug code...
                // printf("Thread %d Message from ", thread_id), print_ip_addr(&ip_hdr->ip6_src);
                // printf("_%d to ", ntohs(udp_hdr->source)), print_ip_addr(&ip_hdr->ip6_dst), printf("_%d\n", ntohs(udp_hdr->dest));
                uint16_t msg_size = ntohs(udp_hdr->len) - UDP_HDRLEN;
                if (rcv_buf != NULL)
                    memcpy(rcv_buf, payload, msg_size);
                if (remote_addr != NULL)
                    memcpy(remote_addr, &ip_hdr->ip6_dst, IPV6_SIZE);
                memcpy(target_ip->sin6_addr.s6_addr, &ip_hdr->ip6_src, IPV6_SIZE);
                target_ip->sin6_port = udp_hdr->source;
                tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                next_packet(&ring_rx_g);
                return msg_size;
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