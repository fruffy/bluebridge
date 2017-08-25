/*
 ** client.c -- a stream socket client demo
 */

#include "network.h"
#include <netdb.h>            // struct addrinfo

struct in6_addr allocateRemoteMem(struct sockaddr_in6 *targetIP);
int writeRemoteMem(struct sockaddr_in6 *targetIP, char *payload,  struct in6_addr *toPointer);
int freeRemoteMem(struct sockaddr_in6 *targetIP,  struct in6_addr *toPointer);
char * getRemoteMem(struct sockaddr_in6 *targetIP, struct in6_addr *toPointer);
int migrateRemoteMem(struct sockaddr_in6 *targetIP, struct in6_addr *toPointer, int machineID);
