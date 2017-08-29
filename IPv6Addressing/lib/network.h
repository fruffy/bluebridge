#ifndef PROJECT_NET
#define PROJECT_NET

#include <netinet/udp.h>      // struct udphdr
#include <netinet/ip6.h>      // struct ip6_hdr

// Define some constants.
#define BLOCK_SIZE 4096 // max number of bytes we can get at once
#define IPV6_SIZE 16
#define POINTER_SIZE sizeof(void*)

#define ALLOC_CMD       "01"
#define WRITE_CMD       "02"
#define GET_CMD         "03"
#define FREE_CMD        "04"
#define GET_ADDR_CMD    "05"

static const int SUBNET_ID = 1; // 16 bits for subnet id
#define NUM_HOSTS 3 // number of hosts in the rack

int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP);
int sendUDPIPv6(int sockfd, char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr  ipv6Pointer);
int sendUDPRaw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP);
int sendUDPIPv6Raw(char * sendBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr  ipv6Pointer);
int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP);
int receiveUDPIPv6(int sockfd, char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer);
int receiveUDPIPv6Raw(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer);

uint64_t getPointerFromIPv6(struct in6_addr addr);
struct in6_addr getIPv6FromPointer(uint64_t pointer);


extern struct udppacket* genPacketInfo();
extern struct sockaddr_in6 *init_rcv_socket();
extern struct sockaddr_in6 *init_rcv_socket_old();
extern void init_send_socket();
extern void close_sockets();
extern int get_rcv_socket();
extern int get_send_socket();
struct addrinfo* bindSocket(struct addrinfo* p, struct addrinfo* servinfo, int* sockfd);

void printSendLat();
#endif