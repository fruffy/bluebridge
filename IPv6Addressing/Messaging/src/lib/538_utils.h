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

#define GLOBAL_ID "EA75:DB1A:68D8" // 48 bits for global id
//#define SUBNET_ID "C75F" // 16 bits for subnet id
#define SUBNET_ID "11"
#define BLOCK_SIZE 4096 // max number of bytes we can get at once
#define POINTER_SIZE sizeof(void*)
#define IPV6_SIZE 16

#define ALLOC_CMD	"01"
#define WRITE_CMD	"02"
#define GET_CMD		"03"
#define FREE_CMD	"04"


void *get_in_addr(struct sockaddr *sa);

int getLine(char *prmpt, char *buff, size_t sz);

unsigned char *gen_rdm_bytestream(size_t num_bytes);

char *get_rdm_string(size_t num_bytes, int index);

int sendTCP(int sockfd, char * sendBuffer, int msgBlockSize);

int receiveTCP(int sockfd, char * receiveBuffer, int msgBlockSize);

int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p);
int sendUDPIPv6(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p);


int receiveUDPLegacy (int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);
int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);
int receiveUDPIPv6(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);

uint64_t getPointerFromString(char* input);

uint64_t getPointerFromIPv6(struct in6_addr addr);

struct in6_addr getIPv6FromPointer(uint64_t pointer);

struct in6_addr getIPv6FromPointerStr(uint64_t pointer);
uint64_t getPointerFromIPv6Str(struct in6_addr addr);



#endif
