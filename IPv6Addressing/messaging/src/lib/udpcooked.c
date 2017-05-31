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

int cookUDP (int sockfd, struct sockaddr_in6* dst_addr, int dst_port, char * data, int datalen) {
     

    int status, frame_length, sd;
    struct ip6_hdr iphdr;
    struct udphdr udphdr;
    uint8_t *src_mac, *dst_mac, *ether_frame;
    struct addrinfo hints;
    struct sockaddr_ll device;
    struct ifreq ifr;
    struct ifaddrs *ifap, *ifa = NULL; 
    struct sockaddr_in6 *sa; 
    char src_ip[INET6_ADDRSTRLEN];
    char dst_ip[INET6_ADDRSTRLEN];
    char * interface = malloc(50);


    int src_port;
    // Allocate memory for various arrays.
    src_mac = (uint8_t *) malloc (6 * sizeof (uint8_t));//allocate_ustrmem (6);
    dst_mac = (uint8_t *) malloc (6 * sizeof (uint8_t));//allocate_ustrmem (6);
    ether_frame = (uint8_t *) malloc (IP_MAXPACKET * sizeof (uint8_t));//allocate_ustrmem (IP_MAXPACKET);
 
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
    if ((memcmp(dst_addr->sin6_addr.s6_addr, sa->sin6_addr.s6_addr,6) == 0)) {
        strcpy(interface,"lo");
    } else {
        strcpy(interface,ifa->ifa_name);
    }

    // TODO: TEST HACK
    // strcpy(interface,"lo");


    struct sockaddr_in6 sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1) {
        perror("getsockname");
        free(interface);
        return EXIT_FAILURE;
    } else {
        src_port = ntohs(sin.sin6_port);
    }
    inet_ntop(AF_INET6, dst_addr->sin6_addr.s6_addr, dst_ip, sizeof dst_ip);

    // TODO: remove HACK
    // memcpy(src_ip, dst_ip, sizeof dst_ip);

    print_debug("Interface %s, Source IP %s, Source Port %d, Destination IP %s, Destination Port %d, Size %d", interface, src_ip, src_port, dst_ip, dst_port, datalen);
    // Submit request for a socket descriptor to look up interface.
    if ((sd = socket (AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror ("socket() failed to get socket descriptor for using ioctl() ");
        exit (EXIT_FAILURE);
    }

    // Use ioctl() to look up interface name and get its MAC address.
    memset (&ifr, 0, sizeof (ifr));
    snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
    if (ioctl (sd, SIOCGIFHWADDR, &ifr) < 0) {
        perror ("ioctl() failed to get source MAC address ");
        return (EXIT_FAILURE);
    }
    close (sd);

    // Copy source MAC address.
    memcpy (src_mac, ifr.ifr_hwaddr.sa_data, 6 * sizeof (uint8_t));

    // Report source MAC address to stdout.
    if (DEBUG) {
        printf ("[DEBUG] MAC address for interface %s is ", interface);
        for (i=0; i<5; i++) {
        printf ("%02x:", src_mac[i]);
        }
        printf ("%02x\n", src_mac[5]);
    }

    // Find interface index from interface name and store index in
    // struct sockaddr_ll device, which will be used as an argument of sendto().
    memset (&device, 0, sizeof (device));
    if ((device.sll_ifindex = if_nametoindex (interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }
    print_debug("Index for interface %s is %i", interface, device.sll_ifindex);

    // Set destination MAC address: you need to fill this out
/*  dst_mac[0] = 0xff;
    dst_mac[1] = 0xff;
    dst_mac[2] = 0xff;
    dst_mac[3] = 0xff;
    dst_mac[4] = 0xff;
    dst_mac[5] = 0xff;*/
    memset(dst_mac, 15, 6);

    // Fill out hints for getaddrinfo().
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = hints.ai_flags | AI_CANONNAME;

    // Fill out sockaddr_ll.
    device.sll_family = AF_PACKET;
    memcpy (device.sll_addr, src_mac, 6 * sizeof (uint8_t));
    device.sll_halen = 6;

    // IPv6 header

    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    iphdr.ip6_flow = htonl ((6 << 28) | (0 << 20) | 0);

    // Payload length (16 bits): UDP header + UDP data
    iphdr.ip6_plen = htons (UDP_HDRLEN + datalen);

    // Next header (8 bits): 17 for UDP
    iphdr.ip6_nxt = IPPROTO_UDP;

    // Hop limit (8 bits): default to maximum value
    iphdr.ip6_hops = 255;

    // Source IPv6 address (128 bits)
    if ((status = inet_pton (AF_INET6, src_ip, &(iphdr.ip6_src))) != 1) {
        fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
        exit (EXIT_FAILURE);
    }

    // Destination IPv6 address (128 bits)
    if ((status = inet_pton (AF_INET6, dst_ip, &(iphdr.ip6_dst))) != 1) {
        fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
        exit (EXIT_FAILURE);
    }

    // UDP header

    // Source port number (16 bits): pick a number
    udphdr.source = htons (src_port);

    // Destination port number (16 bits): pick a number
    udphdr.dest = htons (dst_port);

    // Length of UDP datagram (16 bits): UDP header + UDP data
    udphdr.len = htons (UDP_HDRLEN + datalen);

    // UDP checksum (16 bits)
    udphdr.check = udp6_checksum (iphdr, udphdr, (uint8_t *) data, datalen);

    // Fill out ethernet frame header.

    // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + UDP header + UDP data)
    frame_length = 6 + 6 + 2 + IP6_HDRLEN + UDP_HDRLEN + datalen;

    // Destination and Source MAC addresses
    memcpy (ether_frame, dst_mac, 6 * sizeof (uint8_t));
    memcpy (ether_frame + 6, src_mac, 6 * sizeof (uint8_t));

    // Next is ethernet type code (ETH_P_IPV6 for IPv6).
    // http://www.iana.org/assignments/ethernet-numbers
    ether_frame[12] = ETH_P_IPV6 / 256;
    ether_frame[13] = ETH_P_IPV6 % 256;

    // Next is ethernet frame data (IPv6 header + UDP header + UDP data).

    // IPv6 header
    memcpy (ether_frame + ETH_HDRLEN, &iphdr, IP6_HDRLEN * sizeof (uint8_t));

    // UDP header
    memcpy (ether_frame + ETH_HDRLEN + IP6_HDRLEN, &udphdr, UDP_HDRLEN * sizeof (uint8_t));

    // UDP data
    memcpy (ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, data, datalen * sizeof (uint8_t));

    // Submit request for a raw socket descriptor.
    if ((sd = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL))) < 0) {
        perror ("socket() failed ");
        exit (EXIT_FAILURE);
    }

    // Send ethernet frame to socket.
    if ((sendto (sd, ether_frame, frame_length, 0, (struct sockaddr *) &device, sizeof (device))) <= 0) {
        perror ("sendto() failed");
        exit (EXIT_FAILURE);
    }
    // Close socket descriptor.
    close (sd);

    // Free allocated memory.
    free (src_mac);
    free (dst_mac);
    free (ether_frame);
    free(interface);
/*  free (ifap); 
    free(ifa); 
    free(sa);*/
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
    print_debug("Checksum: %d\n", answer);

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

    return checksum ((uint16_t *) buf, chksumlen);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (int len)
{
    void *tmp;

    if (len <= 0) {
        fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
        exit (EXIT_FAILURE);
    }

    tmp = (char *) malloc (len * sizeof (char));
    if (tmp != NULL) {
        memset (tmp, 0, len * sizeof (char));
        return (tmp);
    } else {
        fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_strmem().\n");
        exit (EXIT_FAILURE);
    }
}

// Allocate memory for an array of unsigned chars.
uint8_t *
allocate_ustrmem (int len)
{
    void *tmp;

    if (len <= 0) {
        fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_ustrmem().\n", len);
        exit (EXIT_FAILURE);
    }

    tmp = (uint8_t *) malloc (len * sizeof (uint8_t));
    if (tmp != NULL) {
        memset (tmp, 0, len * sizeof (uint8_t));
        return (tmp);
    } else {
        fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_ustrmem().\n");
        exit (EXIT_FAILURE);
    }
}
