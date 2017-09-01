/*
 ** client.c -- a stream socket client demo
 */

#include "network.h"


struct in6_addr allocateRemoteMem(struct sockaddr_in6 *targetIP);
int allocateMem(struct sockaddr_in6 *targetIP);
int getMem(struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer);
int writeMem(char * receiveBuffer, struct sockaddr_in6 * targetIP, struct in6_addr * ipv6Pointer);
int freeMem(struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer);

extern struct config get_config(char *filename);