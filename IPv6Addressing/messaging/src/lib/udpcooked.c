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

// Send a "cooked" IPv6 UDP packet via raw socket.
// Need to specify destination MAC address.
// Includes some UDP data.
#include "udpcooked.h"
#include "debug.h"
struct udppacket {
    struct ip6_hdr iphdr;
    struct udphdr udphdr;
    struct sockaddr_ll device;
    uint8_t ether_frame[IP_MAXPACKET];
    char interface[20];
};

static struct udppacket packetinfo;
static int sd;
struct udppacket* genPacketInfo (int sockfd) {
     

    struct ifaddrs *ifap, *ifa = NULL; 
    struct sockaddr_in6 *sa; 
    char src_ip[INET6_ADDRSTRLEN];

    // Allocate memory for various arrays.
    uint8_t src_mac[6];
    uint8_t dst_mac[6];

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
/*    if ((sd = socket (AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror ("socket() failed to get socket descriptor for using ioctl() ");
        exit (EXIT_FAILURE);
    }*/

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
/*    memset (&packetinfo.device, 0, sizeof (packetinfo.device));
    if ((packetinfo.device.sll_ifindex = if_nametoindex (packetinfo.interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }*/

    //TODO: Hardcorded hack, remove
    packetinfo.device.sll_ifindex = 2;
    //print_debug("Index for interface %s is %i", packetinfo.interface, packetinfo.device.sll_ifindex);

    // Set source/destination MAC address:
    memset(dst_mac, 255, 6);
    memset(src_mac, 255, 6);



    // Fill out sockaddr_ll.
    packetinfo.device.sll_family = AF_PACKET;
    memcpy (packetinfo.device.sll_addr, src_mac, 6 * sizeof (uint8_t));
    packetinfo.device.sll_halen = 6;

    // IPv6 header
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    packetinfo.iphdr.ip6_flow = htonl ((6 << 28) | (0 << 20) | 0);

    // Next header (8 bits): 17 for UDP
    packetinfo.iphdr.ip6_nxt = IPPROTO_UDP;

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
    if ((sd = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL))) < 0) {
        perror ("socket() failed ");
        exit (EXIT_FAILURE);
    }
    return &packetinfo;
}

int cookUDP (struct sockaddr_in6* dst_addr, int dst_port, char* data, int datalen) {
    // struct timeval st, et;
    // gettimeofday(&st,NULL);
    int frame_length;
    //Set destination IP
    packetinfo.iphdr.ip6_dst = dst_addr->sin6_addr;


    if (memcmp(&packetinfo.iphdr.ip6_dst, &packetinfo.iphdr.ip6_src, 6) == 0 ) {
        //TODO: Hardcorded hack, remove
        packetinfo.device.sll_ifindex = 1;
    } else {
        packetinfo.device.sll_ifindex = 2;

    }

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
    if ((sendto (sd, packetinfo.ether_frame, frame_length, 0, (struct sockaddr *) &packetinfo.device, sizeof (packetinfo.device))) <= 0) {
        perror ("sendto() failed");
        exit (EXIT_FAILURE);
    }
    // gettimeofday(&et,NULL);
    // int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec);
    // printf("Raw socket send: %d micro seconds\n",elapsed);
    // Close socket descriptor.
    //close (sd);



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

    // TODO: remove this is a check.
    // answer = 50;
    //print_debug("Checksum: %d\n", answer);

    return (answer);
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

   //return checksum ((const char *) buf, chksumlen);
  return checksum ((uint16_t *) buf, chksumlen);
}
