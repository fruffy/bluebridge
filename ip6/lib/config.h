#ifndef PROJECT_CONFIG
#define PROJECT_CONFIG

#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <netinet/ip6.h>    // struct ip6_hdr

struct config {
    char interface[20];
    char server_port[5];
    uint16_t src_port;
    struct in6_addr src_addr;
    int debug;
    int num_hosts;
    struct in6_addr hosts[40];
};

struct config set_bb_config(char *filename, int isServer);
struct config get_bb_config();

#endif
