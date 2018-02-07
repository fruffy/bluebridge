#ifndef IPDUMP_CHECKSUM_H
#define IPDUMP_CHECKSUM_H

#include <stdint.h>
#include <rte_ip.h>

uint16_t
get_ipv4_psd_sum(const struct ipv4_hdr *ip_hdr);

#endif
