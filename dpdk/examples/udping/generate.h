#ifndef IPDUMP_GENERATE_H
#define IPDUMP_GENERATE_H

#include <stdint.h>
#include <rte_mbuf.h>

struct rte_mbuf *construct_udp_packet(unsigned portid, uint16_t src_port, uint16_t dst_port,
    struct ether_addr *dst_mac, uint32_t dst_ip, char *data, uint16_t data_length);

struct rte_mbuf *construct_udp_response(unsigned portid, struct rte_mbuf *m);

#endif
