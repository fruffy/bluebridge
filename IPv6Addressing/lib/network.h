#ifndef PROJECT_NET
#define PROJECT_NET

#include <netinet/udp.h>      // struct udphdr
#include <netinet/ip6.h>      // struct ip6_hdr

#include "config.h"

#define ALLOC_CMD       "01"
#define WRITE_CMD       "02"
#define GET_CMD         "03"
#define FREE_CMD        "04"
#define GET_ADDR_CMD    "05"

struct in6_memaddr {
    uint32_t wildcard;
    uint16_t subid;
    uint16_t src;
    uint64_t paddr;
};

int sendUDPRaw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP);
int sendUDPIPv6Raw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr remoteAddr);
int receiveUDPIPv6Raw(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);

extern struct sockaddr_in6 *init_rcv_socket();
extern struct sockaddr_in6 *init_rcv_socket_old();
extern void init_send_socket();
extern void close_sockets();
struct addrinfo* bindSocket(struct addrinfo* p, struct addrinfo* servinfo, int* sockfd);

void printSendLat();
#endif