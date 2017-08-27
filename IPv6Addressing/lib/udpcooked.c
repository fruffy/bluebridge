/*  Copyright (C) 2011-2015  P.D. Buchan (pdbuchan@yahoo.com)

        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define _GNU_SOURCE

// Send a "cooked" IPv6 UDP packet via raw socket.
// Need to specify destination MAC address.
// Includes some UDP data.
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
#include <signal.h>
#include <pthread.h>
#include <poll.h>

#include "udpcooked.h"
#include "utils.h"


// Function prototypes
uint16_t checksum (uint16_t *, int);
uint16_t udp6_checksum (struct ip6_hdr, struct udphdr, uint8_t *, int);
void init_epoll();
void close_epoll();
void *get_free_buffer();
static int exit_packetsock();

struct udppacket {
    struct ip6_hdr iphdr;
    struct udphdr udphdr;
    struct sockaddr_ll device;
    char ether_frame[IP_MAXPACKET];
    char interface[20];
};

static struct udppacket packetinfo;
static int sd_send;
static int sd_rcv;

void set_interface_name(){
    struct ifaddrs *ifap, *ifa = NULL; 
    struct sockaddr_in6 *sa; 
    char src_ip[INET6_ADDRSTRLEN];
    //TODO: Use config file instead. Avoid memory leaks
    getifaddrs(&ifap); 
    int i = 0; 
    for (ifa = ifap; i<2; ifa = ifa->ifa_next) { 
        if (ifa->ifa_addr->sa_family==AF_INET6) { 
            i++; 
            sa = (struct sockaddr_in6 *) ifa->ifa_addr; 
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), src_ip, 
            sizeof(src_ip), NULL, 0, NI_NUMERICHOST); 
            print_debug("Interface: %s\tAddress: %s", ifa->ifa_name, src_ip); 
        } 
    }
    strcpy(packetinfo.interface,ifa->ifa_name);
    packetinfo.iphdr.ip6_src = sa->sin6_addr; 
}
struct udppacket* genPacketInfo () {


    // Allocate memory for various arrays.
    // Set source/destination MAC address to filler values.
    uint8_t src_mac[6] = { 2 };
    uint8_t dst_mac[6] = { 3 };

    //print_debug("Interface %s, Source IP %s, Source Port %d, Destination IP %s, Destination Port %d, Size %d", interface, src_ip, src_port, dst_ip, dst_port, datalen);
    // Submit request for a socket descriptor to look up interface.
    if ((sd_send = socket (AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror ("socket() failed to get socket descriptor for using ioctl() ");
        exit (EXIT_FAILURE);
    }

    struct sockaddr_in6 sin;
    int src_port;
    socklen_t len = sizeof(sin);
    if (getsockname(sd_rcv, (struct sockaddr *)&sin, &len) == -1) {
        perror("getsockname");
        exit (EXIT_FAILURE);
    } else {
        src_port = ntohs(sin.sin6_port);
    }
    // Source port number (16 bits): pick a number
    packetinfo.udphdr.source = htons (src_port);
    // Find interface index from interface name and store index in
    // struct sockaddr_ll device, which will be used as an argument of sendto().
    memset (&packetinfo.device, 0, sizeof (packetinfo.device));
    if ((packetinfo.device.sll_ifindex = if_nametoindex (packetinfo.interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }

    // Fill out sockaddr_ll.
    packetinfo.device.sll_family = AF_PACKET;
    memcpy (packetinfo.device.sll_addr, src_mac, 6 * sizeof (uint8_t));
    packetinfo.device.sll_halen = 6;

    // IPv6 header
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    packetinfo.iphdr.ip6_flow = htonl ((6 << 28) | (0 << 20) | 0);

    // Next header (8 bits): 17 for UDP
    packetinfo.iphdr.ip6_nxt = IPPROTO_UDP;
    //packetinfo.iphdr.ip6_nxt = 0x9F;

    // Hop limit (8 bits): default to maximum value
    packetinfo.iphdr.ip6_hops = 255;

    // Destination and Source MAC addresses
    memcpy (packetinfo.ether_frame, &dst_mac, 6 * sizeof (uint8_t));
    memcpy (packetinfo.ether_frame + 6, &src_mac, 6 * sizeof (uint8_t));
    // Next is ethernet type code (ETH_P_IPV6 for IPv6).
    // http://www.iana.org/assignments/ethernet-numbers
    packetinfo.ether_frame[12] = ETH_P_IPV6 / 256;
    packetinfo.ether_frame[13] = ETH_P_IPV6 % 256;
    // Submit request for a raw socket descriptor.
    return &packetinfo;
}

/* Initialize a listening socket */
struct sockaddr_in6 *init_rcv_socket(const char *portNumber) {

    struct addrinfo hints, *servinfo, *p = NULL;
    int rv;
    //Socket operator variables
    const int on=1;
    // hints = specifies criteria for selecting the socket address
    // structures
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(NULL, portNumber, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sd_rcv = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
                == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sd_rcv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))
                == -1) {
            perror("setsockopt");
            exit(1);
        }
        setsockopt(sd_rcv, IPPROTO_IPV6, IP_PKTINFO, &on, sizeof(int));
        setsockopt(sd_rcv, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(int));
        setsockopt(sd_rcv, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int));
        struct sockaddr_in6* tempi = (struct sockaddr_in6*) p->ai_addr;
        tempi->sin6_addr = in6addr_any;
        if (bind(sd_rcv, p->ai_addr, p->ai_addrlen) == -1) {
            close(sd_rcv);
            perror("server: bind");
            continue;
        }
        break;
    }
    struct sockaddr_in6 *temp = malloc(sizeof(struct sockaddr_in6));
    memcpy(temp, p->ai_addr, sizeof(struct sockaddr_in6));
    set_interface_name();
    return temp;
}


int get_rcv_socket() {
    return sd_rcv;
}

int get_send_socket() {
    return sd_send;
}

void close_sockets() {
    close(sd_rcv);
    close_epoll();
    exit_packetsock();
}
/// The number of frames in the ring
//  This number is not set in stone. Nor are block_size, block_nr or frame_size
#define CONF_RING_FRAMES        128
#define FRAMESIZE               (4096 + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN + 2 + 32)
#define BLOCKSIZE               (FRAMESIZE) * (CONF_RING_FRAMES)

// tp_block_size must be a multiple of PAGE_SIZE (1)
// tp_frame_size must be greater than TPACKET_HDRLEN (obvious)
// tp_frame_size must be a multiple of TPACKET_ALIGNMENT
// tp_frame_nr   must be exactly frames_per_block*tp_block_nr

/// Offset of data from start of frame
#define PKT_OFFSET      (TPACKET_ALIGN(sizeof(struct tpacket_hdr)) + \
                         TPACKET_ALIGN(sizeof(struct sockaddr_ll)))

/// (unimportant) macro for loud failure
#define RETURN_ERROR(lvl, msg) \
  do {                    \
    fprintf(stderr, msg); \
    return lvl;            \
  } while(0);

static struct sockaddr_ll txring_daddr;

/// create a linklayer destination address
//  @param ringdev is a link layer device name, such as "eth0"
static int init_ring_daddr(int fd, const char *ringdev)
{
  struct ifreq ifreq;

  // get device index
  strcpy(ifreq.ifr_name, ringdev);
  if (ioctl(fd, SIOCGIFINDEX, &ifreq)) {
    perror("ioctl");
    return -1;
  }

  txring_daddr.sll_family    = AF_PACKET;
  txring_daddr.sll_protocol  = htons(ETH_P_ALL);
  txring_daddr.sll_ifindex   = ifreq.ifr_ifindex;

  // set the linklayer destination address
  // NOTE: this should be a real address, not ff.ff....
  txring_daddr.sll_halen     = ETH_ALEN;
  memset(&txring_daddr.sll_addr, 0xff, ETH_ALEN);
  return 0;
}

/// Initialize a packet socket ring buffer
//  @param ringtype is one of PACKET_RX_RING or PACKET_TX_RING
char *ring;
static char * init_packetsock_ring(int fd, int ringtype){
    struct tpacket_req tp;

    // tell kernel to export data through mmap()ped ring
    tp.tp_block_size = BLOCKSIZE;
    tp.tp_block_nr = 1;
    tp.tp_frame_size = FRAMESIZE;
    tp.tp_frame_nr = CONF_RING_FRAMES;
    if (setsockopt(fd, SOL_PACKET, ringtype, (void*) &tp, sizeof(tp)))
    RETURN_ERROR(NULL, "setsockopt() ring\n");
    #ifdef TPACKET_V2
    val = TPACKET_V1;
    setsockopt(fd, SOL_PACKET, PACKET_HDRLEN, &val, sizeof(val));
    #endif


    // open ring
    ring = mmap(0, tp.tp_block_size * tp.tp_block_nr,
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!ring)
    RETURN_ERROR(NULL, "mmap()\n");

    if (init_ring_daddr(fd, packetinfo.interface))
    return NULL;
    return ring;
}

/// Create a packet socket. If param ring is not NULL, the buffer is mapped
//  @param ring will, if set, point to the mapped ring on return
//  @return the socket fd
static int init_packetsock(int ringtype) {
    int fd;

    // open packet socket
    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0)
    RETURN_ERROR(-1, "Root priliveges are required\nsocket() rx. \n");

    init_packetsock_ring(fd, ringtype);

    if (!ring) {
      close(fd);
      return -1;
    }

  return fd;
}

static int exit_packetsock() {
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

/// transmit a packet using packet ring
//  NOTE: for high rate processing try to batch system calls, 
//        by writing multiple packets to the ring before calling send()
//
//  @param pkt is a packet from the network layer up (e.g., IP)
//  @return 0 on success, -1 on failure
static int process_tx(int fd, const char *pkt, int pktlen) {
  static int ring_offset = 0;

  struct tpacket_hdr *header;
  struct pollfd pollset;
  char *off;
  int ret;

  // fetch a frame
  // like in the PACKET_RX_RING case, we define frames to be a page long,
  // including their header. This explains the use of getpagesize().
  header = (void *) ring + (ring_offset * FRAMESIZE);
  //assert((((unsigned long) header) & (FRAMESIZE - 1)) == 0);
  while (header->tp_status != TP_STATUS_AVAILABLE) {

    // if none available: wait on more data
    pollset.fd = fd;
    pollset.events = POLLOUT;
    pollset.revents = 0;
    ret = poll(&pollset, 1, 1000 /* don't hang */);
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
    if (sendto(sd_send, NULL, 0, 0, (struct sockaddr *) &packetinfo.device, sizeof(packetinfo.device)) < 0) {
        perror("sendto failed");
        return -1;
    }

  return 0;
}
char *ring;
void init_send_socket() {

    //Socket operator variables
    const int on=1;

/*    if ((sd_send = socket (AF_PACKET, SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC, htons (ETH_P_ALL))) < 0) {
        perror ("socket() failed ");
        exit (EXIT_FAILURE);
    }
    struct tpacket_req req;
    req.tp_block_size =  4096;
    req.tp_block_nr   =   2;
    req.tp_frame_size =  256;
    req.tp_frame_nr   =   16;
*/
/*    struct ifreq ifr;
    strncpy ((char *) ifr.ifr_name, packetinfo.interface, 20);
    ioctl (sd_send, SIOCGIFINDEX, &ifr);
    bind(sd_send, (struct sockaddr *) &packetinfo.device, sizeof (packetinfo.device) );*/


/*    struct packet_mreq      mr;
    memset (&mr, 0, sizeof (mr));
    mr.mr_ifindex = ifr.ifr_ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    setsockopt (sd_send, SOL_PACKET,PACKET_ADD_MEMBERSHIP, &mr, sizeof (mr));
    setsockopt(sd_send , SOL_PACKET , PACKET_RX_RING , (void*)&req , sizeof(req));
    setsockopt(sd_send , SOL_PACKET , PACKET_TX_RING , (void*)&req , sizeof(req));*/
    init_epoll();
    sd_send = init_packetsock(PACKET_TX_RING);
    setsockopt(sd_send, IPPROTO_IPV6, IP_PKTINFO, &on, sizeof(int));
    setsockopt(sd_send, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(int));
    setsockopt(sd_send, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int));
}
/// Example application that opens a packet socket with rx_ring
int strange_send(char *packet, int datalen) {

    // TODO: make correct IP packet out of pkt
    process_tx(sd_send, packet, datalen);

    return 0;
}

int cookUDP (struct sockaddr_in6* dst_addr, int dst_port, char* data, int datalen) {

    int frame_length;
    //Set destination IP
    packetinfo.iphdr.ip6_dst = dst_addr->sin6_addr;

    //TODO: Hardcorded hack, remove
/*    if (memcmp(&packetinfo.iphdr.ip6_dst, &packetinfo.iphdr.ip6_src, 6) == 0 ) {
        packetinfo.device.sll_ifindex = 1;
    } else {
        packetinfo.device.sll_ifindex = 2;
    }
*/
    // Payload length (16 bits): UDP header + UDP data
    packetinfo.iphdr.ip6_plen = htons (datalen - 96);
    // UDP header
    // Destination port number (16 bits): pick a number
    packetinfo.udphdr.dest = htons (dst_port);
    // Length of UDP datagram (16 bits): UDP header + UDP data
    packetinfo.udphdr.len = htons (datalen - 96 );
    // UDP checksum (16 bits)

    packetinfo.udphdr.check = udp6_checksum (packetinfo.iphdr, packetinfo.udphdr, (uint8_t *) data, datalen);

    // Fill out ethernet frame header.
    // IPv6 header
    memcpy (packetinfo.ether_frame + ETH_HDRLEN, &packetinfo.iphdr, IP6_HDRLEN * sizeof (uint8_t));
    // UDP header
    memcpy (packetinfo.ether_frame + ETH_HDRLEN + IP6_HDRLEN, &packetinfo.udphdr, UDP_HDRLEN * sizeof (uint8_t));
    // UDP data
    memcpy (packetinfo.ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, data, datalen * sizeof (uint8_t));
    // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + UDP header + UDP data)
    frame_length = 6 + 6 + 2 + IP6_HDRLEN + UDP_HDRLEN + datalen;

    // Send ethernet frame to socket.
/*    if ((sendto (sd, packetinfo.ether_frame, frame_length, 0, (struct sockaddr *) &packetinfo.device, sizeof (packetinfo.device))) <= 0) {
        perror ("sendto() failed");
        exit (EXIT_FAILURE);
    }*/
    //write(sd_send,packetinfo.ether_frame, frame_length);
    strange_send (packetinfo.ether_frame, frame_length);
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

struct ep_interface {
    int socket;
    int index;
    char name[255];
    struct tpacket_hdr * first_tpacket_hdr;
    int tpacket_i;
    int mapped_memory_size;
    struct tpacket_req tpacket_req;
};


int get_interface_index(int socket, const char * name)
{
    struct ifreq ifreq;
    memset(&ifreq, 0, sizeof ifreq);

    snprintf(ifreq.ifr_name, sizeof ifreq.ifr_name, "%s", name);

    if (ioctl(socket, SIOCGIFINDEX, &ifreq)) {
        return -1;
    }

    return ifreq.ifr_ifindex;
}


int bind_to_interface(int socket, int interface_index)
{
    struct sockaddr_ll sockaddr_ll;
    memset(&sockaddr_ll, 0, sizeof sockaddr_ll);
    
    sockaddr_ll.sll_family = AF_PACKET;
    sockaddr_ll.sll_protocol = htons(ETH_P_ALL);
    sockaddr_ll.sll_ifindex = interface_index;

    if (-1 == bind(socket, (struct sockaddr *)&sockaddr_ll, sizeof sockaddr_ll)) {
        return -1;
    }

    return 0;
}

struct tpacket_hdr * get_packet(struct ep_interface * interface) {
    return (struct tpacket_hdr *)((char *)interface->first_tpacket_hdr + interface->tpacket_i * interface->tpacket_req.tp_frame_size);
}


/*struct ethhdr * get_ethhdr(struct tpacket_hdr * tpacket_hdr)
{
    return (void *)((char *) tpacket_hdr) + tpacket_hdr->tp_mac;
}*/


struct sockaddr_ll * get_sockaddr_ll(struct tpacket_hdr * tpacket_hdr)
{
    return (struct sockaddr_ll *) ((char *) tpacket_hdr) + TPACKET_ALIGN(sizeof *tpacket_hdr);
}


void next_packet(struct ep_interface * interface) {
    interface->tpacket_i = (interface->tpacket_i + 1) % interface->tpacket_req.tp_frame_nr;
}


int setup_packet_mmap(struct ep_interface * interface) {
    struct tpacket_req tpacket_req = {
        .tp_block_size = 4096,
        .tp_frame_size = 4096,
        .tp_block_nr = 64,
        .tp_frame_nr = 64
    };

    int size = tpacket_req.tp_block_size * tpacket_req.tp_block_nr;

    void * mapped_memory = NULL;

    if (setsockopt(interface->socket, SOL_PACKET, PACKET_RX_RING, &tpacket_req, sizeof tpacket_req)) {
        return -1;
    }

    mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, interface->socket, 0);

    if (MAP_FAILED == mapped_memory) {
        return -1;
    }

    interface->first_tpacket_hdr = mapped_memory;
    interface->mapped_memory_size = size;
    interface->tpacket_req = tpacket_req;

    return 0;
}


int init_interface(struct ep_interface *interface, const char * name)
{
    interface->socket = socket(AF_PACKET, SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC, htons(ETH_P_ALL));
    const int on = 1;
    setsockopt(interface->socket, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int));
    if (-1 == interface->socket) {
        return -1;
    }

    interface->index = get_interface_index(interface->socket, name);

    if (interface->index < 0) {
        close(interface->socket);
        return -1;
    }

    snprintf(interface->name, sizeof interface->name, "%s", name); 

    if (bind_to_interface(interface->socket, interface->index)) {
        close(interface->socket);
        return -1;
    }

    if (setup_packet_mmap(interface)) {
        close(interface->socket);
        return -1;
    }

    return 0;
}


struct ep_interface * create_interface(const char * name) 
{
    struct ep_interface * interface = malloc(sizeof *interface);

    if (!interface) {
        return NULL;
    }

    if (init_interface(interface, name)) {
        free(interface);
        return NULL;
    }

    return interface;
}


void exit_interface(struct ep_interface * interface)
{
    munmap(interface->first_tpacket_hdr, interface->mapped_memory_size);
    close(interface->socket);
}


void destroy_interface(struct ep_interface * interface)
{
    exit_interface(interface);
    free(interface);
}

/*          struct sockaddr_in6 {
               sa_family_t     sin6_family;   AF_INET6
               in_port_t       sin6_port;     port number
               uint32_t        sin6_flowinfo; IPv6 flow information
               struct in6_addr sin6_addr;     IPv6 address
               uint32_t        sin6_scope_id; Scope ID (new in 2.4)};
*/
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
int epoll_fd = -1;
struct ep_interface * interface_ep;
void init_epoll() {
    interface_ep = create_interface(packetinfo.interface);
    struct epoll_event event1 = {
        .events = EPOLLIN,
        .data = { .ptr = NULL }
    };

    if (!interface_ep) {
        perror("create_interface failed.");
       exit(1);
    }

    epoll_fd = epoll_create(1024);

    if (-1 == epoll_fd) {
        perror("epoll_create failed.");
        destroy_interface(interface_ep);
       exit(1);
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, interface_ep->socket, &event1)) {
        perror("epoll_ctl failed");
        close(epoll_fd);
        destroy_interface(interface_ep);
       exit(1);
    }
}

void close_epoll(){
    close(epoll_fd);
    destroy_interface(interface_ep);
}

int strangeReceive(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr *ipv6Pointer) {
    while (1) {
        struct epoll_event events[1];
        int num_events = epoll_wait(epoll_fd, events, sizeof events / sizeof *events, -1);
        if (num_events == -1)  {
            if (errno == EINTR)  {
                perror("epoll_wait returned -1");
                break;
            }
            perror("error");
            continue;
        }
        for (int i = 0; i < num_events; ++i)  {
            struct epoll_event * event = &events[i];
            if (event->events & EPOLLIN) {
                struct tpacket_hdr *tpacket_hdr = get_packet(interface_ep);
                //struct sockaddr_ll *sockaddr_ll = NULL;

                if (tpacket_hdr->tp_status & TP_STATUS_COPY) {
                    next_packet(interface_ep);
                    continue;
                }

                if (tpacket_hdr->tp_status & TP_STATUS_LOSING) {
                    next_packet(interface_ep);
                    continue;
                }
                
                struct ethhdr *ethhdr = (void *)((char *) tpacket_hdr) + tpacket_hdr->tp_mac;
                struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
                struct udphdr *udphdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
                char* payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
                if (!memcmp(&udphdr->dest, &packetinfo.udphdr.source, sizeof(uint16_t))) {
                    memcpy(receiveBuffer,payload, msgBlockSize);
                    if (ipv6Pointer != NULL)
                        memcpy(ipv6Pointer->s6_addr,&iphdr->ip6_dst,IPV6_SIZE);
                    memcpy(targetIP->sin6_addr.s6_addr,&iphdr->ip6_src,IPV6_SIZE);
                    memcpy(&targetIP->sin6_port,&udphdr->source,sizeof(uint16_t));
                    //char s[INET6_ADDRSTRLEN];
                    /*inet_ntop(targetIP->sin6_family, &targetIP->sin6_addr, s, sizeof s);
                    print_debug("Got message from %s:%d", s, ntohs(targetIP->sin6_port));*/
                    udphdr->dest = 0;
                    return msgBlockSize;
                }
                tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                next_packet(interface_ep);
            }
        }
    }
    return 0;
}

int cooked_receive(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr *ipv6Pointer) {
    struct sockaddr_in6 from;
    struct iovec iovec[1];
    struct msghdr msg;
    char msg_control[1024];
    char udp_packet[msgBlockSize];
    int numbytes = 0;
    //char s[INET6_ADDRSTRLEN];
    iovec[0].iov_base = udp_packet;
    iovec[0].iov_len = sizeof(udp_packet);
    msg.msg_name = &from;
    msg.msg_namelen = sizeof(from);
    msg.msg_iov = iovec;
    msg.msg_iovlen = sizeof(iovec) / sizeof(*iovec);
    msg.msg_control = msg_control;
    msg.msg_controllen = sizeof(msg_control);
    msg.msg_flags = 0;

    print_debug("Waiting for response...");
    memset(receiveBuffer, 0, msgBlockSize);
    numbytes = recvmsg(sd_rcv, &msg, 0);
    struct in6_pktinfo * in6_pktinfo;
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != 0; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
            in6_pktinfo = (struct in6_pktinfo*)CMSG_DATA(cmsg);
            //inet_ntop(targetIP->sin6_family, &in6_pktinfo->ipi6_addr, s, sizeof s);
            //print_debug("Received packet was sent to this IP %s",s);
            if (ipv6Pointer != NULL)
                memcpy(ipv6Pointer->s6_addr,&in6_pktinfo->ipi6_addr,IPV6_SIZE);
            memcpy(receiveBuffer,iovec[0].iov_base,iovec[0].iov_len);
            memcpy(targetIP, (struct sockaddr *) &from, sizeof(from));
        }
    }

    //inet_ntop(targetIP->sin6_family, targetIP, s, sizeof s);
    //print_debug("Got message from %s:%d", s, ntohs(targetIP->sin6_port));
    //printNBytes(receiveBuffer, 50);
    return numbytes;
}

// Build IPv6 UDP pseudo-header and call checksum function (Section 8.1 of RFC 2460).
uint16_t udp6_checksum (struct ip6_hdr iphdr, struct udphdr udphdr, uint8_t *payload, int payloadlen) {
    char buf[IP_MAXPACKET];
    char *ptr;
    int chksumlen = 0;
    int i;

    ptr = &buf[0];  // ptr points to beginning of buffer buf

    // Copy source IP address into buf (128 bits)
    memcpy (ptr, &iphdr.ip6_src.s6_addr, sizeof (iphdr.ip6_src.s6_addr));
    ptr += sizeof (iphdr.ip6_src.s6_addr);
    chksumlen += sizeof (iphdr.ip6_src.s6_addr);

    // Copy destination IP address into buf (128 bits)
    memcpy (ptr, &iphdr.ip6_dst.s6_addr, sizeof (iphdr.ip6_dst.s6_addr));
    ptr += sizeof (iphdr.ip6_dst.s6_addr);
    chksumlen += sizeof (iphdr.ip6_dst.s6_addr);

    // Copy UDP length into buf (32 bits)
    memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
    ptr += sizeof (udphdr.len);
    chksumlen += sizeof (udphdr.len);

    // Copy zero field to buf (24 bits)
    *ptr = 0; ptr++;
    *ptr = 0; ptr++;
    *ptr = 0; ptr++;
    chksumlen += 3;

    // Copy next header field to buf (8 bits)
    memcpy (ptr, &iphdr.ip6_nxt, sizeof (iphdr.ip6_nxt));
    ptr += sizeof (iphdr.ip6_nxt);
    chksumlen += sizeof (iphdr.ip6_nxt);

    // Copy UDP source port to buf (16 bits)
    memcpy (ptr, &udphdr.source, sizeof (udphdr.source));
    ptr += sizeof (udphdr.source);
    chksumlen += sizeof (udphdr.source);

    // Copy UDP destination port to buf (16 bits)
    memcpy (ptr, &udphdr.dest, sizeof (udphdr.dest));
    ptr += sizeof (udphdr.dest);
    chksumlen += sizeof (udphdr.dest);

    // Copy UDP length again to buf (16 bits)
    memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
    ptr += sizeof (udphdr.len);
    chksumlen += sizeof (udphdr.len);

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
