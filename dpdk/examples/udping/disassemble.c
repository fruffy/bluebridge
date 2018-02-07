#include <stdio.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include <rte_byteorder.h>
#include <rte_mbuf.h>

#include "disassemble.h"

extern uint32_t host_ip;
extern uint32_t dest_ip;
extern uint64_t rx_ping_count[];

static int
is_broadcast_address(struct ether_addr *addr) {
    int i;
    for(i = 0; i < 6; i ++) {
        if (addr->addr_bytes[i] != 0xff) return 0;
    }
    return 1;
}

static int
ipdump_disassemble_udp_packet(struct rte_mbuf *m, unsigned portid,
                              struct ipv4_hdr *ipv4_hdr, struct udp_hdr *udp_hdr,
                              unsigned char *end)
{
    uint8_t *payload = (uint8_t *) udp_hdr + sizeof(struct udp_hdr);
    uint16_t src_port = rte_be_to_cpu_16(udp_hdr->src_port);
    uint16_t dst_port = rte_be_to_cpu_16(udp_hdr->dst_port);
    uint16_t length = rte_be_to_cpu_16(udp_hdr->dgram_len);
    rx_ping_count[rte_lcore_id()] = *(uint64_t *) payload;

#ifdef UDP_DBG
    printf("[Core %d] Packet length: %d\n", rte_lcore_id(), rte_pktmbuf_pkt_len(m));
    printf("    UDP source port %d dest port %d, payload %ld\n",
        src_port, dst_port, *(uint64_t *) payload);
#endif

    return 0;
}

static int
ipdump_disassemble_ipv4_packet(struct rte_mbuf *m, unsigned portid,
        struct ipv4_hdr *ipv4_hdr, unsigned char *end)
{
#if 0
    char srcip[80], destip[80];
    inet_ntop(AF_INET, &ipv4_hdr->src_addr, srcip, sizeof(srcip));
    inet_ntop(AF_INET, &ipv4_hdr->dst_addr, destip, sizeof(destip));
    printf("    IPv4 source %s dest %s length %d\n", srcip, destip,
            rte_be_to_cpu_16(ipv4_hdr->total_length));
#endif

    switch(ipv4_hdr->next_proto_id) {

        case IPPROTO_UDP: {
            struct udp_hdr *udp_hdr = (struct udp_hdr *) end;
            end += sizeof(*udp_hdr);
            return ipdump_disassemble_udp_packet(m, portid, ipv4_hdr, udp_hdr, end);
            break;
        }

        default:
            break;
    }

    return -1;
}

static int
ipdump_disassemble_eth_packet(struct rte_mbuf *m, unsigned portid,
                              struct ether_hdr *eth, uint16_t ether_type,
                              unsigned char *end)
{
    uint16_t type = rte_be_to_cpu_16(ether_type);
    switch(type)  {
        case ETHER_TYPE_IPv4: {
            struct ipv4_hdr *ipv4_hdr = (struct ipv4_hdr *)end;
            end += sizeof(*ipv4_hdr);
            return ipdump_disassemble_ipv4_packet(m, portid, ipv4_hdr, end);
            break;
        }

        default:
#if 0
            printf("    (unknown type 0x%04x)\n", type);
#endif
            break;
    }

    return -1;
}

/* Prints packet info to the screen (unless the packet is to be filtered out). */
int
ipdump_disassemble_packet(struct rte_mbuf *m, unsigned portid,
                          int enable_filter)
{
    struct ether_hdr *eth = rte_pktmbuf_mtod(m, struct ether_hdr *);
    unsigned char *end;

    if(enable_filter) {
        struct ether_addr this_addr;
        rte_eth_macaddr_get(portid, &this_addr);
        if(memcmp(eth->d_addr.addr_bytes, this_addr.addr_bytes, 6) != 0
                && !is_broadcast_address(&eth->d_addr)) {
            return -1;  /* filter out */
        }
    }

#if 0
    char source[80], dest[80];
    format_mac_address(source, &eth->s_addr);
    format_mac_address(dest, &eth->d_addr);
    //printf("ipdump(%d): packet from %s to %s\n", portid, source, dest);
#endif

    end = rte_pktmbuf_mtod(m, unsigned char*) + sizeof(struct ether_hdr);
    return ipdump_disassemble_eth_packet(m, portid, eth, eth->ether_type, end);
}

