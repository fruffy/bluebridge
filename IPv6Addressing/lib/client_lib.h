#ifndef PROJECT_CLIENT
#define PROJECT_CLIENT

#include <netdb.h>            // struct addrinfo
#include "network.h"


struct in6_addr allocateRemoteMem(struct sockaddr_in6 *targetIP);
int writeRemoteMem(struct sockaddr_in6 *targetIP, char *payload,  struct in6_addr *toPointer);
int freeRemoteMem(struct sockaddr_in6 *targetIP,  struct in6_addr *toPointer);
char * getRemoteMem(struct sockaddr_in6 *targetIP, struct in6_addr *toPointer);
int migrateRemoteMem(struct sockaddr_in6 *targetIP, struct in6_addr *toPointer, int machineID);

struct in6_addr *gen_rdm_IPv6Target();
void set_host_list(struct in6_addr *host_addrs, int num_hosts);

#endif