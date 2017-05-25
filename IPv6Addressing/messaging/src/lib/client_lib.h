/*
 ** client.c -- a stream socket client demo
 */

#include "538_utils.h"
#include "debug.h"

struct in6_addr allocateRemoteMem(int sockfd, struct addrinfo * p);
int writeRemoteMem(int sockfd, struct addrinfo * p, char * payload,  struct in6_addr * toPointer);
int freeRemoteMem(int sockfd, struct addrinfo * p,  struct in6_addr * toPointer);
char * getRemoteMem(int sockfd, struct addrinfo * p, struct in6_addr * toPointer);
int migrateRemoteMem(int sockfd, struct addrinfo * p, struct in6_addr * toPointer, int machineID);
