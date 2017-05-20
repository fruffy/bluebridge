/*
 ** client.c -- a stream socket client demo
 */

#include "538_utils.h"
#include "debug.h"

struct in6_addr allocateMem(int sockfd, struct addrinfo * p);
int writeToMemory(int sockfd, struct addrinfo * p, char * payload,  struct in6_addr * toPointer);
int releaseMemory(int sockfd, struct addrinfo * p,  struct in6_addr * toPointer);
char * getMemory(int sockfd, struct addrinfo * p, struct in6_addr * toPointer);
int migrate(int sockfd, struct addrinfo * p, struct in6_addr * toPointer, int machineID);
