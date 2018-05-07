#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netinet/udp.h>      // struct udphdr
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <errno.h>            // errno, perror()
#include <sys/mman.h>         // mmap()


#include "udp_raw_common.h"
#include "../utils.h"
#include "../config.h"

// Function prototypes
uint16_t checksum (uint16_t *, int);
uint16_t udp6_checksum (struct ip6_hdr *, struct udphdr *, uint8_t *, int);

static __thread unsigned char ether_frame[IP_MAXPACKET];
static __thread int sd_send;
static __thread char *ring_tx;
static __thread int ring_offset = 0;
static __thread int thread_id_tx;

struct sockaddr_ll configure_tx(struct config *cfg) {

    struct ip6_hdr iphdr;
    struct udphdr udphdr;
    struct sockaddr_ll iface;
    // Find interface index from interface name and store index in
    // struct sockaddr_ll device, which will be used as an argument of sendto().
    memset (&iface, 0, sizeof (iface));
    if ((iface.sll_ifindex = if_nametoindex (cfg->interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }
    //memcpy(src_mac, packetinfo.device.sll_addr, 6); 
    uint8_t src_mac[6] = { 0xa0, 0x36, 0x9f, 0x45, 0xd8, 0x74 };
    uint8_t dst_mac[6] = { 0xa0, 0x36, 0x9f, 0x45, 0xd8, 0x75 };
    // Fill out sockaddr_ll.
    iface.sll_family = AF_PACKET;
    memcpy (iface.sll_addr, src_mac, 6 * sizeof (uint8_t));
    iface.sll_halen = 6;

    // Destination and Source MAC addresses
    memcpy (ether_frame, &dst_mac, 6 * sizeof (uint8_t));
    memcpy (ether_frame + 6, &src_mac, 6 * sizeof (uint8_t));
    // Next is ethernet type code (ETH_P_IPV6 for IPv6).
    // http://www.iana.org/assignments/ethernet-numbers
    //ether_frame[12] = 0xFF;
    //ether_frame[13] = 0xFF;
    ether_frame[12] = ETH_P_IPV6 / 256;
    ether_frame[13] = ETH_P_IPV6 % 256;

    // IPv6 header
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    iphdr.ip6_src = cfg->src_addr;
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    iphdr.ip6_flow = htonl ((6 << 28) | (0 << 20) | 0);

    // Next header (8 bits): 17 for UDP
    iphdr.ip6_nxt = IPPROTO_UDP;
    //packetinfo.iphdr.ip6_nxt = 0x9F;

    // Hop limit (8 bits): default to maximum value
    iphdr.ip6_hops = 255;
    //printf("Setting Source UDP to %d \n",ntohs(cfg->src_port) + thread_id_tx );
    udphdr.source = htons(ntohs(cfg->src_port) + thread_id_tx);
    memcpy (ether_frame + ETH_HDRLEN, &iphdr, IP6_HDRLEN * sizeof (uint8_t));
    memcpy (ether_frame + ETH_HDRLEN + IP6_HDRLEN, &udphdr, UDP_HDRLEN * sizeof (uint8_t));
    return iface;
}

// tp_block_size must be a multiple of PAGE_SIZE (1)
// tp_frame_size must be greater than TPACKET_HDRLEN (obvious)
// tp_frame_size must be a multiple of TPACKET_ALIGNMENT
// tp_frame_nr   must be exactly frames_per_block*tp_block_nr

/// Initialize a packet socket ring buffer
//  @param ringtype is one of PACKET_RX_RING or PACKET_TX_RING
int init_packetsock_ring(int sd){
    struct tpacket_req tp;

    // tell kernel to export data through mmap()ped ring
    tp.tp_block_size = C_BLOCKSIZE;
    tp.tp_block_nr = 1;
    tp.tp_frame_size = C_FRAMESIZE;
    tp.tp_frame_nr = C_RING_FRAMES;
    if (setsockopt(sd, SOL_PACKET, PACKET_TX_RING, (void*) &tp, sizeof(tp)))
        RETURN_ERROR(-1, "setsockopt() ring_tx\n");
    int on = 1;
    //setsockopt(sd, SOL_PACKET, PACKET_QDISC_BYPASS, &on, sizeof(on));

    int val = TPACKET_V3;
    setsockopt(sd, SOL_PACKET, PACKET_HDRLEN, &val, sizeof(val));
    setsockopt(sd, SOL_PACKET, SO_BUSY_POLL, &on, sizeof(on));
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on,sizeof(int));
    int size = tp.tp_block_size *tp.tp_block_nr;
/*    if (setsockopt(sd, SOL_PACKET, PACKET_LOSS, &on, sizeof(int)))
        RETURN_ERROR(-1, "setsockopt: PACKET_LOSS");*/
/*    if (setsockopt(sd, SOL_PACKET, SO_SNDBUF, &size, on))
        RETURN_ERROR(-1, "setsockopt: SO_SNDBUF");*/

    // open ring
    ring_tx = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, sd, 0);
    if (!ring_tx)
        RETURN_ERROR(-1, "mmap()\n");
    return EXIT_SUCCESS;
}

/// Create a packet socket. If param ring is not NULL, the buffer is mapped
//  @param ring will, if set, point to the mapped ring on return
//  @return the socket fd
int init_packetsock() {
    // open packet socket
    sd_send = socket(AF_PACKET, SOCK_RAW|SOCK_NONBLOCK, htons(ETH_P_ALL));
    if (sd_send < 0)
        RETURN_ERROR(-1, "Root privileges are required\nsocket() rx. \n");

    if (init_packetsock_ring(sd_send))
        RETURN_ERROR(-1, "init_packetsock");

    if (!ring_tx) {
        close(sd_send);
        RETURN_ERROR(-1, "Ring initialisation failed!\n");
    }
    return EXIT_SUCCESS;
}

int get_tx_socket() {
    return sd_send;
}

void set_thread_id_tx(int id) {
    thread_id_tx = id;
}

int close_tx_socket() {
    if (munmap(ring_tx, C_RING_BLOCKS * C_BLOCKSIZE)) {
        perror("munmap");
        return 1;
    }
    if (close(sd_send)) {
        perror("close");
        return 1;
    }
    sd_send = -1;
    ring_tx = NULL;
    ring_offset = 0;

    return 0;
}

void init_tx_socket(struct config *cfg) {
    struct sockaddr_ll iface = configure_tx(cfg);
    init_packetsock();
    if (-1 == bind(sd_send, (struct sockaddr *)&iface, sizeof(iface))) {
        perror("Send: Could not bind socket.");
        exit(1);
    }
}

/// transmit a packet using packet ring
//  NOTE: for high rate processing try to batch system calls, 
//        by writing multiple packets to the ring before calling send()
//
//  @param pkt is a packet from the network layer up (e.g., IP)
//  @return 0 on success, -1 on failure
int send_mmap(unsigned const char *pkt, int pktlen) {
    // fetch a frame
    // like in the PACKET_RX_RING case, we define frames to be a page long,
    // including their header. This explains the use of getpagesize().
    struct tpacket_hdr *header = (struct tpacket_hdr * )((char *) ring_tx + (ring_offset * C_FRAMESIZE));
    //assert((((unsigned long) header) & (FRAMESIZE - 1)) == 0);
    // fill data
    char *off = ((char *) header) + (TPACKET_HDRLEN - sizeof(struct sockaddr_ll));
    memcpy(off, pkt, pktlen);
    // fill header
    header->tp_len = pktlen;
    header->tp_status = TP_STATUS_SEND_REQUEST;
    /*    printf(KRED"Thread %d Sending on socket %d and offset %d\n"RESET, thread_id, sd_send, ring_offset);*/
    // increase consumer ring pointer
    ring_offset = (ring_offset + 1) & (C_RING_FRAMES - 1);
    // notify kernel
    //write(sd_send,ether_frame, 0);
    return EXIT_SUCCESS;
}

int flush_buffer() {
    while (1) {
        if (sendto(sd_send, NULL, 0, MSG_DONTWAIT, (struct sockaddr *)NULL, sizeof(struct sockaddr_ll)) < 0) {
            perror( "sendto failed");
            printf("Retrying....\n");
        } else {
            return EXIT_SUCCESS;
        }
    }
}

int cooked_batched_send(struct pkt_rqst *pkts, int num_pkts, uint32_t *sub_ids) {

    for (int i= 0; i < num_pkts; i++) {
        struct pkt_rqst pkt = pkts[i];
        struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ether_frame + ETH_HDRLEN);
        struct udphdr *udphdr = (struct udphdr *)((char *)ether_frame + ETH_HDRLEN + IP6_HDRLEN);
        pkt.dst_addr.args = pkt.dst_addr.args | sub_ids[pkt.dst_addr.subid]<<16;
        //Set destination IP
        iphdr->ip6_dst = *(struct in6_addr *)&pkt.dst_addr;
        // Payload length (16 bits): UDP header + UDP data
        iphdr->ip6_plen = htons (UDP_HDRLEN + pkt.datalen);
        // UDP header
        // Destination port number (16 bits): pick a number
        // We expect the port to already be in network byte order
        udphdr->dest = pkt.dst_port;
        // Length of UDP datagram (16 bits): UDP header + UDP data
        udphdr->len = htons (UDP_HDRLEN + pkt.datalen);
        // Static UDP checksum
        udphdr->check = 0xFFAA;
        //udphdr->check = udp6_checksum (iphdr, udphdr, (uint8_t *) data, datalen);
        // Copy our data
        memcpy (ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, pkt.data, pkt.datalen * sizeof (uint8_t));
        // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + UDP header + UDP data)
        int frame_length = ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN + pkt.datalen;
        /*    char dst_ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6,&iphdr->ip6_dst, dst_ip, sizeof dst_ip);
        printf("Sending to part two %s::%d\n", dst_ip, ntohs(udphdr->dest));*/
        if (i >= 16384) {
            flush_buffer();
        }
        send_mmap(ether_frame, frame_length);
    }
    flush_buffer();
    return EXIT_SUCCESS;
}

int cooked_send(struct pkt_rqst pkt) {

    struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ether_frame + ETH_HDRLEN);
    struct udphdr *udphdr = (struct udphdr *)((char *)ether_frame + ETH_HDRLEN + IP6_HDRLEN);
    //Set destination IP
    iphdr->ip6_dst = *(struct in6_addr *)&pkt.dst_addr;
    // Payload length (16 bits): UDP header + UDP data
    iphdr->ip6_plen = htons (UDP_HDRLEN + pkt.datalen);
    // UDP header
    // Destination port number (16 bits): pick a number
    // We expect the port to already be in network byte order
    udphdr->dest = pkt.dst_port;
    // Length of UDP datagram (16 bits): UDP header + UDP data
    udphdr->len = htons (UDP_HDRLEN + pkt.datalen);
    // Static UDP checksum
    udphdr->check = 0xFFAA;
    //udphdr->check = udp6_checksum (iphdr, udphdr, (uint8_t *) data, datalen);
    // Copy our data
    // printf("Memcpy: %s, ether_frame: ", pkt.data);
    // print_n_bytes(ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, pkt.datalen);

    memcpy (ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, pkt.data, pkt.datalen * sizeof (uint8_t));
    // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + UDP header + UDP data)
    int frame_length = ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN + pkt.datalen;
    /*    char dst_ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6,&iphdr->ip6_dst, dst_ip, sizeof dst_ip);
    printf("Sending to part two %s::%d\n", dst_ip, ntohs(udphdr->dest));*/
    // Place the packet in the ring buffer
    // printf("Send mmap\n");
    send_mmap(ether_frame, frame_length);
    // printf("Flushing buffer\n");
    // Flush the buffer
    flush_buffer();
    // printf("returning\n");
    return EXIT_SUCCESS;
}

// DEPRECATED
// Computing the internet checksum (RFC 1071).
// Note that the internet checksum does not preclude collisions.
uint16_t checksum (uint16_t *addr, int len) {
    int count = len;
    register uint32_t sum = 0;
    uint16_t answer = 0;

    // Sum up 2-byte values until none or only one byte left.
    while (count > 1) {
        sum += *(addr++);
        count -= 2;
    }

    // Add left-over byte, if any.
    if (count > 0) {
        sum += *(uint8_t *) addr;
    }

    // Fold 32-bit sum into 16 bits; we lose information by doing this,
    // increasing the chances of a collision.
    // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    // Checksum is one's compliment of sum.
    answer = ~sum;

    if (answer == 0) {
        answer = 0xffff;
    }

    return (answer);
}

// DEPRECATED
// Build IPv6 UDP pseudo-header and call checksum function (Section 8.1 of RFC 2460).
uint16_t udp6_checksum (struct ip6_hdr *iphdr, struct udphdr *udphdr, uint8_t *payload, int payloadlen) {
    char buf[IP_MAXPACKET];
    char *ptr;
    int chksumlen = 0;
    int i;

    ptr = &buf[0];  // ptr points to beginning of buffer buf

    // Copy source IP address into buf (128 bits)
    memcpy (ptr, &iphdr->ip6_src.s6_addr, sizeof (iphdr->ip6_src.s6_addr));
    ptr += sizeof (iphdr->ip6_src.s6_addr);
    chksumlen += sizeof (iphdr->ip6_src.s6_addr);

    // Copy destination IP address into buf (128 bits)
    memcpy (ptr, &iphdr->ip6_dst.s6_addr, sizeof (iphdr->ip6_dst.s6_addr));
    ptr += sizeof (iphdr->ip6_dst.s6_addr);
    chksumlen += sizeof (iphdr->ip6_dst.s6_addr);

    // Copy UDP length into buf (32 bits)
    memcpy (ptr, &udphdr->len, sizeof (udphdr->len));
    ptr += sizeof (udphdr->len);
    chksumlen += sizeof (udphdr->len);

    // Copy zero field to buf (24 bits)
    *ptr = 0; ptr++;
    *ptr = 0; ptr++;
    *ptr = 0; ptr++;
    chksumlen += 3;

    // Copy next header field to buf (8 bits)
    memcpy (ptr, &iphdr->ip6_nxt, sizeof (iphdr->ip6_nxt));
    ptr += sizeof (iphdr->ip6_nxt);
    chksumlen += sizeof (iphdr->ip6_nxt);

    // Copy UDP source port to buf (16 bits)
    memcpy (ptr, &udphdr->source, sizeof (udphdr->source));
    ptr += sizeof (udphdr->source);
    chksumlen += sizeof (udphdr->source);

    // Copy UDP destination port to buf (16 bits)
    memcpy (ptr, &udphdr->dest, sizeof (udphdr->dest));
    ptr += sizeof (udphdr->dest);
    chksumlen += sizeof (udphdr->dest);

    // Copy UDP length again to buf (16 bits)
    memcpy (ptr, &udphdr->len, sizeof (udphdr->len));
    ptr += sizeof (udphdr->len);
    chksumlen += sizeof (udphdr->len);

    // Copy UDP checksum to buf (16 bits)
    // Zero, since we don't know it yet
    *ptr = 0; ptr++;
    *ptr = 0; ptr++;
    chksumlen += 2;

    // Copy payload to buf
    memcpy (ptr, payload, payloadlen * sizeof (uint8_t));
    ptr += payloadlen;
    chksumlen += payloadlen;

    // Pad to the next 16-bit boundary
    for (i=0; i<payloadlen%2; i++, ptr++) {
        *ptr = 0;
        ptr++;
        chksumlen++;
    }

  return checksum ((uint16_t *) buf, chksumlen);
}
