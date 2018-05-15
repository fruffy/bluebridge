#ifndef PROJECT_NET
#define PROJECT_NET

#include <netinet/udp.h>      // struct udphdr
#include <netinet/ip6.h>      // struct ip6_hdr
#include "types.h"
#include "config.h"

int send_udp_raw(uint8_t * sendBuffer, int msgBlockSize, ip6_memaddr *remote_addr, int dst_port);
int send_udp_raw_batched(pkt_rqst *pkts, uint32_t *sub_ids, int num_addrs);
int rcv_udp6_raw(uint8_t * receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr);
int rcv_udp6_raw_id(uint8_t * receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr);
void rcv_udp6_raw_bulk(int num_packets);
struct sockaddr_in6 *init_sockets(struct config *configstruct, int server);
void close_sockets();
struct sockaddr_in6 *init_net_thread(int t_id, struct config *bb_conf, int isServer);
void printSendLat();
void launch_server_loop(struct config *bb_conf);
#endif