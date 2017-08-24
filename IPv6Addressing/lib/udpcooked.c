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

#include "udpcooked.h"
#include "utils.h"

// Function prototypes
uint16_t checksum (uint16_t *, int);
uint16_t udp6_checksum (struct ip6_hdr, struct udphdr, uint8_t *, int);

struct udppacket {
    struct ip6_hdr iphdr;
    struct udphdr udphdr;
    struct sockaddr_ll device;
    uint8_t ether_frame[IP_MAXPACKET];
    char interface[20];
};

static struct udppacket packetinfo;
static int sd_send;
static int sd_rcv;

struct udppacket* genPacketInfo (int sockfd) {
    struct ifaddrs *ifap, *ifa = NULL; 
    struct sockaddr_in6 *sa; 
    char src_ip[INET6_ADDRSTRLEN];

    // Allocate memory for various arrays.
    // Set source/destination MAC address to filler values.
    uint8_t src_mac[6] = { 2 };
    uint8_t dst_mac[6] = { 3 };

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
    packetinfo.iphdr.ip6_src = sa->sin6_addr; 
    strcpy(packetinfo.interface,ifa->ifa_name);
    //print_debug("Interface %s, Source IP %s, Source Port %d, Destination IP %s, Destination Port %d, Size %d", interface, src_ip, src_port, dst_ip, dst_port, datalen);
    // Submit request for a socket descriptor to look up interface.
    if ((sd_send = socket (AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror ("socket() failed to get socket descriptor for using ioctl() ");
        exit (EXIT_FAILURE);
    }

    struct sockaddr_in6 sin;
    int src_port;
    socklen_t len = sizeof(sin);
    if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1) {
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

int openRawSocket() {

    //Socket operator variables
    const int on=1, off=0;

    if ((sd_send = socket (packetinfo.device.sll_family, SOCK_RAW, htons (ETH_P_ALL))) < 0) {
        perror ("socket() failed ");
        exit (EXIT_FAILURE);
    }
    struct tpacket_req req;
    req.tp_block_size =  4096;
    req.tp_block_nr   =   2;
    req.tp_frame_size =  256;
    req.tp_frame_nr   =   16;

    struct ifreq ifr;
    strncpy ((char *) ifr.ifr_name, packetinfo.interface, 20);
    ioctl (sd_send, SIOCGIFINDEX, &ifr);

    bind ( sd_send, (struct sockaddr *) &packetinfo.device, sizeof (packetinfo.device) );
    struct packet_mreq      mr;
    memset (&mr, 0, sizeof (mr));
    mr.mr_ifindex = ifr.ifr_ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    setsockopt (sd_send, SOL_PACKET,PACKET_ADD_MEMBERSHIP, &mr, sizeof (mr));
    setsockopt(sd_send , SOL_PACKET , PACKET_RX_RING , (void*)&req , sizeof(req));
    setsockopt(sd_send , SOL_PACKET , PACKET_TX_RING , (void*)&req , sizeof(req));
    setsockopt(sd_send, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
    setsockopt(sd_send, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
    setsockopt(sd_send, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

    return EXIT_SUCCESS;
}

void closeRawSocket() {
    close(sd_send);
}

int getRawSocket() {
    return sd_send;
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
    packetinfo.iphdr.ip6_plen = htons (UDP_HDRLEN + datalen);
    // UDP header
    // Destination port number (16 bits): pick a number
    packetinfo.udphdr.dest = htons (dst_port);
    // Length of UDP datagram (16 bits): UDP header + UDP data
    packetinfo.udphdr.len = htons (UDP_HDRLEN + datalen);
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
     write(sd_send,packetinfo.ether_frame, frame_length);

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

int cooked_receive(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP) {
    
    struct sockaddr_in6 from;
    struct iovec iovec[1];
    struct msghdr msg;
    char msg_control[1024];
    char udp_packet[msgBlockSize];
    int numbytes = 0;
    char s[INET6_ADDRSTRLEN];
    iovec[0].iov_base = udp_packet;
    iovec[0].iov_len = sizeof(udp_packet);
    msg.msg_name = &from;
    msg.msg_namelen = sizeof(from);
    msg.msg_iov = iovec;
    msg.msg_iovlen = sizeof(iovec) / sizeof(*iovec);
    msg.msg_control = msg_control;
    msg.msg_controllen = sizeof(msg_control);
    msg.msg_flags = 0;
    int sockfd;
    int sockopt;
    struct ifreq ifopts;    /* set promiscuous mode */
    struct ifreq if_ip; /* get ip addr */
        /* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
    if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(0x86DD))) == -1) {
        perror("listener: socket"); 
        return -1;
    }

    /* Set interface to promiscuous mode - do we need to do this every time? */
    strncpy(ifopts.ifr_name, packetinfo.interface, IFNAMSIZ-1);
    ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
    ifopts.ifr_flags |= IFF_PROMISC;
    ioctl(sockfd, SIOCSIFFLAGS, &ifopts);
    /* Allow the socket to be reused - incase connection is closed prematurely */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    /* Bind to device */
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, packetinfo.interface, IFNAMSIZ-1) == -1)  {
        perror("SO_BINDTODEVICE");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    //03000000000002000000000086dd600000001008
    while (1) {
        print_debug("Waiting for response...");
        memset(receiveBuffer, 0, msgBlockSize);
        numbytes = recvfrom(sd_rcv, &msg, msgBlockSize, 0, NULL, NULL);
        printNBytes((char*)&msg, 50);
        struct in6_pktinfo * in6_pktinfo;
        struct cmsghdr* cmsg;
        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != 0; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
                in6_pktinfo = (struct in6_pktinfo*)CMSG_DATA(cmsg);
                inet_ntop(targetIP->sin6_family, &in6_pktinfo->ipi6_addr, s, sizeof s);
                print_debug("Received packet was sent to this IP %s",s);
                memcpy(&targetIP->sin6_addr.s6_addr,&in6_pktinfo->ipi6_addr, 16);
                memcpy(receiveBuffer,iovec[0].iov_base,iovec[0].iov_len);
                memcpy(targetIP, (struct sockaddr *) &from, sizeof(from));
            }
        }

        inet_ntop(targetIP->sin6_family, targetIP, s, sizeof s);
        print_debug("Got message from %s:%d", s, ntohs(targetIP->sin6_port));
    }
    return numbytes;
}
#define ETHER_TYPE  0x86DD
#define DEST_MAC0   0x00
#define DEST_MAC1   0x00
#define DEST_MAC2   0x00
#define DEST_MAC3   0x00
#define DEST_MAC4   0x00
#define DEST_MAC5   0x00

int cooked_receive_failed(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP) {

        char sender[INET6_ADDRSTRLEN];
        int sockfd, ret, i;
        int sockopt;
        ssize_t numbytes;
        struct ifreq ifopts;    /* set promiscuous mode */
        struct ifreq if_ip; /* get ip addr */
        struct sockaddr_in6 their_addr;
        uint8_t buf[msgBlockSize];

        /* Header structures */
        struct ether_header *eh = (struct ether_header *) buf;
        struct ip6_hdr *iph = (struct ip6_hdr *) (buf + sizeof(struct ether_header));
        struct udphdr *udph = (struct udphdr *) (buf + sizeof(struct iphdr) + sizeof(struct ether_header));

        memset(&if_ip, 0, sizeof(struct ifreq));

        /* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
        if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETHER_TYPE))) == -1) {
            perror("listener: socket"); 
            return -1;
        }

        /* Set interface to promiscuous mode - do we need to do this every time? */
        strncpy(ifopts.ifr_name, packetinfo.interface, IFNAMSIZ-1);
        ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
        ifopts.ifr_flags |= IFF_PROMISC;
        ioctl(sockfd, SIOCSIFFLAGS, &ifopts);
        /* Allow the socket to be reused - incase connection is closed prematurely */
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
            perror("setsockopt");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        /* Bind to device */
        if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, packetinfo.interface, IFNAMSIZ-1) == -1)  {
            perror("SO_BINDTODEVICE");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

    repeat: printf("listener: Waiting to recvfrom...\n");
            numbytes = read(sockfd, buf, 0);
            printf("listener: got packet %lu bytes\n", numbytes);

            /* Check the packet is for me */
            if (eh->ether_dhost[0] == DEST_MAC0 &&
                    eh->ether_dhost[1] == DEST_MAC1 &&
                    eh->ether_dhost[2] == DEST_MAC2 &&
                    eh->ether_dhost[3] == DEST_MAC3 &&
                    eh->ether_dhost[4] == DEST_MAC4 &&
                    eh->ether_dhost[5] == DEST_MAC5) {
                printf("Correct destination MAC address\n");
            } else {
                printf("Wrong destination MAC: %x:%x:%x:%x:%x:%x\n",
                                eh->ether_dhost[0],
                                eh->ether_dhost[1],
                                eh->ether_dhost[2],
                                eh->ether_dhost[3],
                                eh->ether_dhost[4],
                                eh->ether_dhost[5]);
                ret = -1;
                goto done;
            }

            /* Get source IP */
            memcpy(&their_addr.sin6_addr.s6_addr, &iph->ip6_src, 16);
            inet_ntop(AF_INET, &((struct sockaddr_in*)&their_addr)->sin_addr, sender, sizeof sender);

    /*        //Look up my device IP addr if possible 
            strncpy(if_ip.ifr_name, ifName, IFNAMSIZ-1);
            if (ioctl(sockfd, SIOCGIFADDR, &if_ip) >= 0) {// if we can't check then don't 
                printf("Source IP: %s\n My IP: %s\n", sender, 
                        inet_ntoa(((struct sockaddr_in *)&if_ip.ifr_addr)->sin_addr));
                // ignore if I sent it 
                if (strcmp(sender, inet_ntoa(((struct sockaddr_in *)&if_ip.ifr_addr)->sin_addr)) == 0)  {
                    printf("but I sent it :(\n");
                    ret = -1;
                    goto done;
                }
            }*/

            /* UDP payload length */
            ret = ntohs(udph->len) - sizeof(struct udphdr);

            /* Print packet */
            printf("\tData:");
            for (i=0; i<numbytes; i++) printf("%02x:", buf[i]);
            printf("\n");

    done:   goto repeat;

        close(sockfd);
    return ret;
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
