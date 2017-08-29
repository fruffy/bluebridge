#ifndef PROJECT_CONFIG
#define PROJECT_CONFIG

#define _GNU_SOURCE

#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <netinet/ip6.h>    // struct ip6_hdr


struct udppacket {
    struct ip6_hdr iphdr;
    struct udphdr udphdr;
    struct sockaddr_ll device;
    unsigned char ether_frame[IP_MAXPACKET];
    char interface[20];
};

struct udppacket* getPacketInfo();

#endif