#ifndef PROJECT_NET
#define PROJECT_NET

#include <netinet/udp.h>      // struct udphdr
#include <netinet/ip6.h>      // struct ip6_hdr

#include "config.h"
#include "udpcooked.h"

#define ALLOC_CMD       01
#define WRITE_CMD       02
#define GET_CMD         03
#define FREE_CMD        04
#define GET_ADDR_CMD    05
#define CMD_SIZE        02


int send_udp_raw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP);
int send_udp6_raw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int rcv_udp6_raw(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int rcv_udp6_raw_id(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);

extern struct sockaddr_in6 *init_rcv_socket();
extern struct sockaddr_in6 *init_rcv_socket_old();
extern void init_send_socket();
extern void close_sockets();
struct addrinfo* bindSocket(struct addrinfo* p, struct addrinfo* servinfo, int* sockfd);

void printSendLat();
#endif