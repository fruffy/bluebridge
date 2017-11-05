#ifndef PROJECT_CLIENT
#define PROJECT_CLIENT

#include <netdb.h>            // struct addrinfo
#include "network.h"


struct in6_memaddr allocateRemoteMem(struct sockaddr_in6 *targetIP);
int writeRemoteMem(struct sockaddr_in6 *targetIP, char *payload,  struct in6_memaddr *remoteAddr);
int freeRemoteMem(struct sockaddr_in6 *targetIP,  struct in6_memaddr *remoteAddr);
char *getRemoteMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int migrateRemoteMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int machineID);

struct in6_addr *gen_rdm_IPv6Target();
void set_host_list(struct in6_addr *host_addrs, int num_hosts);

#endif