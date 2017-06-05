#ifndef PROJECT_UTILS
#define PROJECT_UTILS

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <netinet/ip6.h>      // struct ip6_hdr
#include <netinet/udp.h>      // struct udphdr
#include <sys/ioctl.h>        // macro ioctl is defined
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <net/ethernet.h>
#include <ifaddrs.h>

// Define some constants.
#define ETH_HDRLEN 14  // Ethernet header length
#define IP6_HDRLEN 40  // IPv6 header length
#define UDP_HDRLEN  8  // UDP header length, excludes data

// Function prototypes
uint16_t checksum (uint16_t *, int);
uint16_t udp6_checksum (struct ip6_hdr, struct udphdr, uint8_t *, int);

int cookUDP (struct sockaddr_in6* dst_addr, int dst_port, char* data, int datalen);
struct udppacket* genPacketInfo (int sockfd);





#define BLOCK_SIZE 4096 // max number of bytes we can get at once
#define POINTER_SIZE sizeof(void*)
#define IPV6_SIZE 16

#define ALLOC_CMD		"01"
#define WRITE_CMD		"02"
#define GET_CMD			"03"
#define FREE_CMD		"04"
#define GET_ADDR_CMD	"05"
const int NUM_HOSTS;

void *get_in_addr(struct sockaddr *sa);

int getLine(char *prmpt, char *buff, size_t sz);

unsigned char *gen_rdm_bytestream(size_t num_bytes);

struct in6_addr * gen_rdm_IPv6Target();
struct in6_addr * gen_fixed_IPv6Target(uint8_t rndHost);


int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p);
int sendUDPIPv6(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p, struct in6_addr  ipv6Pointer);
int sendUDPRaw(char * sendBuffer, int msgBlockSize, struct addrinfo * p);
int sendUDPIPv6Raw(char * sendBuffer, int msgBlockSize, struct addrinfo * p, struct in6_addr  ipv6Pointer);

int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);
int receiveUDPIPv6(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p, struct in6_addr * ipv6Pointer);

uint64_t getPointerFromIPv6(struct in6_addr addr);
struct in6_addr getIPv6FromPointer(uint64_t pointer);
/*
 * TODO: explain
 * Binds to the next available address?
 * Need to bind to INADDR_ANY instead
 */
struct addrinfo* bindSocket(struct addrinfo* p, struct addrinfo* servinfo, int* sockfd);

static inline uint64_t getns(void)
{
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(ret == 0);
    return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}


#endif
