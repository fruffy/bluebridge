#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netdb.h>            // struct addrinfo
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_UDP, INET6_ADDRSTRLEN
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <net/ethernet.h>
#include <ifaddrs.h>
#include <errno.h>            // errno, perror()
#include <sys/mman.h>
#include <sys/epoll.h>
#include <poll.h>


#include "udpcooked.h"
#include "utils.h"
#include "config.h"

// tp_block_size must be a multiple of PAGE_SIZE (1)
// tp_frame_size must be greater than TPACKET_HDRLEN (obvious)
// tp_frame_size must be a multiple of TPACKET_ALIGNMENT
// tp_frame_nr   must be exactly frames_per_block*tp_block_nr

/// Offset of data from start of frame
#define PKT_OFFSET      (TPACKET_ALIGN(sizeof(struct tpacket_hdr)) + \
                         TPACKET_ALIGN(sizeof(struct sockaddr_ll)))

// Function prototypes
uint16_t checksum (uint16_t *, int);
uint16_t udp6_checksum (struct ip6_hdr *, struct udphdr *, uint8_t *, int);


static int sd_send;
static struct packetconfig *packetinfo;
static char *ring;



// tp_block_size must be a multiple of PAGE_SIZE (1)
// tp_frame_size must be greater than TPACKET_HDRLEN (obvious)
// tp_frame_size must be a multiple of TPACKET_ALIGNMENT
// tp_frame_nr   must be exactly frames_per_block*tp_block_nr

/// Initialize a packet socket ring buffer
//  @param ringtype is one of PACKET_RX_RING or PACKET_TX_RING
int init_packetsock_ring(){
    struct tpacket_req tp;

    // tell kernel to export data through mmap()ped ring
    tp.tp_block_size = BLOCKSIZE;
    tp.tp_block_nr = CONF_RING_BLOCKS;
    tp.tp_frame_size = FRAMESIZE;
    tp.tp_frame_nr = CONF_RING_BLOCKS * (BLOCKSIZE/ FRAMESIZE);
    if (setsockopt(sd_send, SOL_PACKET, PACKET_TX_RING, (void*) &tp, sizeof(tp)))
        RETURN_ERROR(-1, "setsockopt() ring\n");
    #ifdef TPACKET_V2
    val = TPACKET_V1;
    setsockopt(sd_send, SOL_PACKET, PACKET_HDRLEN, &val, sizeof(val));
    #endif


    // open ring
    ring = mmap(0, tp.tp_block_size * tp.tp_block_nr,
               PROT_READ | PROT_WRITE, MAP_SHARED, sd_send, 0);
    if (!ring)
        RETURN_ERROR(-1, "mmap()\n");
    return EXIT_SUCCESS;
}

/// Create a packet socket. If param ring is not NULL, the buffer is mapped
//  @param ring will, if set, point to the mapped ring on return
//  @return the socket fd
static int init_packetsock() {

    // open packet socket
    sd_send = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sd_send < 0)
        RETURN_ERROR(-1, "Root privileges are required\nsocket() rx. \n");

    init_packetsock_ring();

    if (!ring) {
      close(sd_send);
        RETURN_ERROR(-1, "Ring initialisation failed!\n");
    }

  return EXIT_SUCCESS;
}

/// transmit a packet using packet ring
//  NOTE: for high rate processing try to batch system calls, 
//        by writing multiple packets to the ring before calling send()
//
//  @param pkt is a packet from the network layer up (e.g., IP)
//  @return 0 on success, -1 on failure
static int send_mmap(unsigned const char *pkt, int pktlen) {
    static int ring_offset = 0;

    struct tpacket_hdr *header;
    struct pollfd pollset;
    char *off;

    // fetch a frame
    // like in the PACKET_RX_RING case, we define frames to be a page long,
    // including their header. This explains the use of getpagesize().
    header = (struct tpacket_hdr * )((char *) ring + (ring_offset * FRAMESIZE));
    //assert((((unsigned long) header) & (FRAMESIZE - 1)) == 0);
    while (header->tp_status != TP_STATUS_AVAILABLE) {
        // if none available: wait on more data
        pollset.fd = sd_send;
        pollset.events = POLLOUT;
        pollset.revents = 0;
        int ret = poll(&pollset, 1, 1000 /* don't hang */);
        if (ret < 0) {
            if (errno != EINTR) {
                perror("poll");
                return -1;
            }
            return 0;
        }
    }
    // fill data
    off = ((char *) header) + (TPACKET_HDRLEN - sizeof(struct sockaddr_ll));
    memcpy(off, pkt, pktlen);
    // fill header
    header->tp_len = pktlen;
    header->tp_status = TP_STATUS_SEND_REQUEST;

    // increase consumer ring pointer
    ring_offset = (ring_offset + 1) & (CONF_RING_FRAMES - 1);
    // notify kernel
    if (sendto(sd_send, NULL, 0, 0, (struct sockaddr *) &packetinfo->device, sizeof(packetinfo->device)) < 0)
        RETURN_ERROR(-1, "sendto failed!\n");
  return 0;
}

int get_send_socket() {
    return sd_send;
}

int close_send_socket() {
    if (munmap(ring, BLOCKSIZE)) {
        perror("munmap");
        return 1;
    }
    if (close(sd_send)) {
        perror("close");
        return 1;
    }
    return 0;
}

void init_send_socket() {

    packetinfo = get_packet_info();
    init_packetsock();
    bind(sd_send, (struct sockaddr *) &packetinfo->device, sizeof (packetinfo->device) );

}

void init_send_socket_old() {

    //Socket operator variables
    const int on=1;

    if ((sd_send = socket (AF_PACKET, SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC, htons (ETH_P_ALL))) < 0) {
        perror ("socket() failed ");
        exit (EXIT_FAILURE);
    }
    struct tpacket_req req;
    req.tp_block_size =  4096;
    req.tp_block_nr   =   2;
    req.tp_frame_size =  256;
    req.tp_frame_nr   =   16;


    bind(sd_send, (struct sockaddr *) &packetinfo->device, sizeof (packetinfo->device) );


    struct packet_mreq      mr;
    memset (&mr, 0, sizeof (mr));
    mr.mr_ifindex = packetinfo->device.sll_ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    setsockopt (sd_send, SOL_PACKET,PACKET_ADD_MEMBERSHIP, &mr, sizeof (mr));
    setsockopt(sd_send , SOL_PACKET , PACKET_RX_RING , (void*)&req , sizeof(req));
    setsockopt(sd_send , SOL_PACKET , PACKET_TX_RING , (void*)&req , sizeof(req));
    setsockopt(sd_send, IPPROTO_IPV6, IP_PKTINFO, &on, sizeof(int));
    setsockopt(sd_send, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(int));
    setsockopt(sd_send, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int));
}


int cooked_send(struct sockaddr_in6* dst_addr, int dst_port, char* data, int datalen) {

    struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)packetinfo->ether_frame + ETH_HDRLEN);
    struct udphdr *udphdr = (struct udphdr *)((char *)packetinfo->ether_frame + ETH_HDRLEN + IP6_HDRLEN);
    //Set destination IP
    iphdr->ip6_dst = dst_addr->sin6_addr;
    // Payload length (16 bits): UDP header + UDP data
    iphdr->ip6_plen = htons (UDP_HDRLEN + datalen);
    // UDP header
    // Destination port number (16 bits): pick a number
    udphdr->dest = htons (dst_port);
    // Length of UDP datagram (16 bits): UDP header + UDP data
    udphdr->len = htons (UDP_HDRLEN + datalen);
    udphdr->check = 0xFFAA;
    // UDP checksum (16 bits)
    //udphdr->check = udp6_checksum (iphdr, udphdr, (uint8_t *) data, datalen);
    // Copy our data
    memcpy (packetinfo->ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, data, datalen * sizeof (uint8_t));
    // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + UDP header + UDP data)
    int frame_length = 6 + 6 + 2 + IP6_HDRLEN + UDP_HDRLEN + datalen;
    // Send ethernet frame to socket.
    //    if ((sendto (sd, packetinfo->ether_frame, frame_length, 0, (struct sockaddr *) &packetinfo->device, sizeof (packetinfo->device))) <= 0) {
    //     perror ("sendto() failed");
    //    exit (EXIT_FAILURE);
    //}
    //write(sd_send,packetinfo->ether_frame, frame_length);
    send_mmap(packetinfo->ether_frame, frame_length);
    return (EXIT_SUCCESS);
}

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
