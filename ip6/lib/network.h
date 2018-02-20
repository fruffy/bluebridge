#ifndef PROJECT_NET
#define PROJECT_NET

#include <netinet/udp.h>      // struct udphdr
#include <netinet/ip6.h>      // struct ip6_hdr

#include "udpcooked.h"

#define ALLOC_CMD       01
#define WRITE_CMD       02
#define GET_CMD         03
#define FREE_CMD        04
#define GET_ADDR_CMD    05
#define ALLOC_BULK_CMD  06
#define CMD_SIZE        02

#define BLOCK_SIZE 4096 // default number of bytes we send

int send_udp_raw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP);
int send_udp6_raw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int rcv_udp6_raw(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int rcv_udp6_raw_id(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);

struct sockaddr_in6 *init_sockets(struct config *configstruct, int server);
void close_sockets();
struct sockaddr_in6 *init_net_thread(int t_id, struct config *bb_conf, int isServer);
void printSendLat();
void launch_server_loop(struct config *bb_conf);
#endif