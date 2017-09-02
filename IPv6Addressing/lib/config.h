#ifndef PROJECT_CONFIG
#define PROJECT_CONFIG

#define _GNU_SOURCE

#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <netinet/ip6.h>    // struct ip6_hdr

// Define some constants.
#define BLOCK_SIZE 4096 // max number of bytes we can get at once
#define IPV6_SIZE 16
#define POINTER_SIZE sizeof(void*)

static const int SUBNET_ID = 1; // 16 bits for subnet id

struct config {
    char interface[20];
    char server_port[5];
    uint16_t src_port;
    struct in6_addr src_addr;
    int debug;
    int num_hosts;
    struct in6_addr hosts[40];
};

struct config configure_bluebridge(char *filename, int isServer);

#endif