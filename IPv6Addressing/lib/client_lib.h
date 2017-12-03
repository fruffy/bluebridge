#ifndef PROJECT_CLIENT
#define PROJECT_CLIENT

#define MAX_HOSTS  5

#include <netdb.h>            // struct addrinfo
#include "network.h"


struct in6_memaddr allocateRemoteMem(struct sockaddr_in6 *targetIP);
int writeRemoteMem(struct sockaddr_in6 *targetIP, char *payload,  struct in6_memaddr *remoteAddr);
int freeRemoteMem(struct sockaddr_in6 *targetIP,  struct in6_memaddr *remoteAddr);
char *getRemoteMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr);
int migrateRemoteMem(struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int machineID);


int writeRaidMem(struct sockaddr_in6 *targetIP, int hosts, char (*payload)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed);
int readRaidMem(struct sockaddr_in6 *targetIP, int hosts, char (*bufs)[MAX_HOSTS][BLOCK_SIZE], struct in6_memaddr **remoteAddrs, int needed);
struct in6_addr *gen_rdm_IPv6Target();
struct in6_addr *gen_IPv6Target();
void set_host_list(struct in6_addr *host_addrs, int num_hosts);


struct in6_addr *get_IPv6Target(uint8_t index);
int numHosts();

#endif
