#ifndef PROJECT_SERVER
#define PROJECT_SERVER
#include "network.h"

int allocateMem(struct sockaddr_in6 *targetIP);
int getMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int writeMem(char * receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int freeMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);


#endif