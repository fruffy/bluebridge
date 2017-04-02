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
#define SUBNET_ID "C75F" // 16 bits for subnet id
#define BLOCK_SIZE 4096 // max number of bytes we can get at once

void *get_in_addr(struct sockaddr *sa);

int getLine(char *prmpt, char *buff, size_t sz);

unsigned char *gen_rdm_bytestream(size_t num_bytes);

char *get_rdm_string(size_t num_bytes, int index);

int sendTCP(int sockfd, char * sendBuffer, int msgBlockSize);

int receiveTCP(int sockfd, char * receiveBuffer, int msgBlockSize);

int sendUDP(int sockfd, char * sendBuffer, int msgBlockSize, struct addrinfo * p);

int receiveUDPLegacy (int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);
int receiveUDP(int sockfd, char * receiveBuffer, int msgBlockSize, struct addrinfo * p);

uint64_t getPointerFromString(char* input);

uint64_t getPointerFromIPv6(struct in6_addr addr);

struct in6_addr getIPv6FromPointer(uint64_t pointer);

#endif
