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
#include <signal.h>
#include <poll.h>
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include "config.h"

// Define some constants.
#define ETH_HDRLEN 14  // Ethernet header length
#define IP6_HDRLEN 40  // IPv6 header length
#define UDP_HDRLEN 8  // UDP header length, excludes data
struct packetconfig packetinfo;

void get_interface_name(){
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
        }
    }
    packetinfo.iphdr.ip6_src = sa->sin6_addr; 
}

int set_interface_ip(struct config *configstruct) {
    struct ifreq ifr;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    strncpy(ifr.ifr_name, configstruct->interface, IFNAMSIZ);
    struct sockaddr_in6* addr = (struct sockaddr_in6*)&ifr.ifr_addr;
    ifr.ifr_addr.sa_family = AF_INET6;
    memcpy(&addr->sin6_addr,&configstruct->src_addr, IPV6_SIZE);
    ioctl(fd, SIOCSIFADDR, &ifr);

    ioctl(fd, SIOCGIFFLAGS, &ifr);
    strncpy(ifr.ifr_name, configstruct->interface, IFNAMSIZ);
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

    ioctl(fd, SIOCSIFFLAGS, &ifr);
    close(fd);
    return 0;
}

struct packetconfig *gen_packet_info(struct config *configstruct, int isServer) {

    // Allocate memory for various arrays.

    // Find interface index from interface name and store index in
    // struct sockaddr_ll device, which will be used as an argument of sendto().
    memset (&packetinfo.device, 0, sizeof (packetinfo.device));
    if ((packetinfo.device.sll_ifindex = if_nametoindex (configstruct->interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }
    // Set source/destination MAC address to filler values.
    uint8_t src_mac[6] = { 2 };
    uint8_t dst_mac[6] = { 3 };
    // Fill out sockaddr_ll.
    packetinfo.device.sll_family = AF_PACKET;
    memcpy (packetinfo.device.sll_addr, src_mac, 6 * sizeof (uint8_t));
    packetinfo.device.sll_halen = 6;

    // Destination and Source MAC addresses
    memcpy (packetinfo.ether_frame, &dst_mac, 6 * sizeof (uint8_t));
    memcpy (packetinfo.ether_frame + 6, &src_mac, 6 * sizeof (uint8_t));
    // Next is ethernet type code (ETH_P_IPV6 for IPv6).
    // http://www.iana.org/assignments/ethernet-numbers
    packetinfo.ether_frame[12] = ETH_P_IPV6 / 256;
    packetinfo.ether_frame[13] = ETH_P_IPV6 % 256;

    // IPv6 header
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    packetinfo.iphdr.ip6_src = configstruct->src_addr;
    set_interface_ip(configstruct);
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    packetinfo.iphdr.ip6_flow = htonl ((6 << 28) | (0 << 20) | 0);

    // Next header (8 bits): 17 for UDP
    packetinfo.iphdr.ip6_nxt = IPPROTO_UDP;
    //packetinfo.iphdr.ip6_nxt = 0x9F;

    // Hop limit (8 bits): default to maximum value
    packetinfo.iphdr.ip6_hops = 255;

    int port = 0;
    if (isServer) {
        printf("This is a server port\n");
        port = atoi(configstruct->server_port);
    }
    if (!port) {
        struct sockaddr_in6 sin;
        socklen_t len = sizeof(sin);
        int sd_send;
        if ((sd_send = socket (AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
            perror ("socket() failed to get socket descriptor for using ioctl() ");
            exit (EXIT_FAILURE);
        }
        if (getsockname(sd_send, (struct sockaddr *)&sin, &len) == -1) {
            perror("getsockname");
            close(sd_send);
            exit (EXIT_FAILURE);
        } else {
            packetinfo.udphdr.source = htons(sin.sin6_port);
            close(sd_send);
        }
    } else {
        printf("This is a server port\n");
        packetinfo.udphdr.source = htons(port);
    }
    memcpy (packetinfo.ether_frame + ETH_HDRLEN, &packetinfo.iphdr, IP6_HDRLEN * sizeof (uint8_t));
    memcpy (packetinfo.ether_frame + ETH_HDRLEN + IP6_HDRLEN, &packetinfo.udphdr, UDP_HDRLEN * sizeof (uint8_t));
    // UDP data
    //memcpy (packetinfo->ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, data, datalen * sizeof (uint8_t));
    return &packetinfo;
}

struct packetconfig *get_packet_info() {
    return &packetinfo;
}

#define DELIM "="
#define SEP ","
struct config get_config(char *filename) {
    struct config configstruct;

    printf("Opening...%s\n",filename );
    FILE *file = fopen (filename, "r");
    if (file != NULL) {
        char line[1024];
        int i = 0;
        while(fgets(line, sizeof(line), file) != NULL) {
            char *cfline;
            cfline = strstr((char *)line,DELIM);
            cfline = cfline + strlen(DELIM);
            cfline[strcspn(cfline, "\n")] = 0;
            if (i == 0){
                    memcpy(configstruct.interface,cfline,20);
            } else if (i == 1){
                char *token;
                token = strtok(cfline, SEP);
                int j = 0;
                while (token != NULL) {
                        inet_pton(AF_INET6, token, configstruct.hosts[j].s6_addr);
                        token = strtok (NULL, ",");
                        j++;
                    }
                configstruct.num_hosts = j;
            } else if (i == 2){
                    memcpy(configstruct.server_port,cfline,strlen(cfline));
            } else if (i == 3){
                    memcpy(configstruct.src_port,cfline,strlen(cfline));
            } else if (i == 4){
                    inet_pton(AF_INET6, cfline, configstruct.src_addr.s6_addr);
            } else if (i == 5){
                    configstruct.debug = atoi(cfline);
            }
            i++;
        }
        printf("Loading Config...\n");
        printf("*** Interface %s\n", configstruct.interface);
        printf("*** Server port %s\n", configstruct.server_port);
        printf("*** Client port %s\n", configstruct.src_port);
        printf("*** Debug active? %d\n", configstruct.debug);
        printf("*** %d target hosts:\t ", configstruct.num_hosts);
        for (int j =0; j < configstruct.num_hosts; j++) {
            char s[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &configstruct.hosts[j], s, sizeof s);
            printf("%s ",s);
        }
        printf("\n");
        fclose(file);
        return configstruct;
    } else {
        perror("Could not find or open file!");
        exit(1);
    }
}
