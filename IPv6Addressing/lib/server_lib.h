#ifndef PROJECT_SERVER
#define PROJECT_SERVER
#include "network.h"

int allocateMem(struct sockaddr_in6 *targetIP);
int getMem(struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer);
int writeMem(char * receiveBuffer, struct sockaddr_in6 * targetIP, struct in6_addr * ipv6Pointer);
int freeMem(struct sockaddr_in6 *targetIP, struct in6_addr * ipv6Pointer);


#endif