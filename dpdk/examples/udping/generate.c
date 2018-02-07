#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include <rte_byteorder.h>
#include <rte_memcpy.h>

#include "generate.h"
#include "checksum.h"

#define HEADER_PTR(var, type, end) \
    var = (type *)end, end += sizeof(type)

extern struct rte_mempool *l2fwd_pktmbuf_pool;
extern uint32_t host_ip;

static uint16_t packet_id = 0;  // counter for packet ids

struct rte_mbuf *construct_udp_packet(unsigned portid, uint16_t src_port, uint16_t dst_port,
    struct ether_addr *dst_mac, uint32_t dst_ip, char *data, uint16_t data_length)
{
    struct rte_mbuf *m = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);

    if (m == NULL) {
        return NULL;
    }

    char *end = rte_pktmbuf_mtod(m, char *);

    struct ether_hdr *ether_hdr;
    struct vlan_hdr *vlan_hdr;
    struct ipv4_hdr *ipv4_hdr;
    struct udp_hdr *udp_hdr;

    HEADER_PTR(ether_hdr, struct ether_hdr, end);
    //HEADER_PTR(vlan_hdr, struct vlan_hdr, end);
    HEADER_PTR(ipv4_hdr, struct ipv4_hdr, end);
    HEADER_PTR(udp_hdr, struct udp_hdr, end);

    ether_addr_copy(dst_mac, &ether_hdr->d_addr);
    rte_eth_macaddr_get(portid, &ether_hdr->s_addr);
    ether_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

#if 0
    ether_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
    vlan_hdr->vlan_tci = rte_cpu_to_be_16(VLAN_ID);
    vlan_hdr->eth_proto = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
#endif

    ipv4_hdr->version_ihl = (4 << 4)  // version 4 = IPv4
        | ((uint8_t)sizeof(struct ipv4_hdr) / 4);  // len
    ipv4_hdr->type_of_service = 0;  // normal, no TOS hacking
    ipv4_hdr->total_length = rte_cpu_to_be_16(
        sizeof(*ipv4_hdr) + sizeof(*udp_hdr) + data_length);
    packet_id += 27;
    ipv4_hdr->packet_id = rte_cpu_to_be_16(packet_id);
    ipv4_hdr->fragment_offset = 0;  // don't deal with fragmentation
    ipv4_hdr->time_to_live = 16;  // reasonably short: small network (uint8_t)
    ipv4_hdr->next_proto_id = IPPROTO_UDP;  // 8-bit value, no endianness
    ipv4_hdr->hdr_checksum = 0;
    ipv4_hdr->src_addr = host_ip;
    ipv4_hdr->dst_addr = rte_cpu_to_be_32(dst_ip);

    udp_hdr->src_port = rte_cpu_to_be_16(src_port);
    udp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
    udp_hdr->dgram_len = rte_cpu_to_be_16(data_length + sizeof(*udp_hdr));
    udp_hdr->dgram_cksum = 0;
    udp_hdr->dgram_cksum = get_ipv4_psd_sum(ipv4_hdr);
    rte_memcpy(end, data, data_length);
    end += data_length;

#if 0
    // compute UDP checksum (sum of pseudo-ip header, udp header, and data)
    uint16_t ipv4_checksum = checksum(ipv4_hdr, sizeof(*ipv4_hdr));
    uint16_t data_length_rounded = (data_length + 1) & ~1;
    uint32_t udp_checksum = partial_sum_ip_pseudo_header(ipv4_hdr);
    udp_checksum = partial_checksum(udp_hdr, sizeof(*udp_hdr), udp_checksum);
    udp_checksum = partial_checksum(data, data_length_rounded, udp_checksum);

    ipv4_hdr->hdr_checksum = ipv4_checksum;
    udp_hdr->dgram_cksum = finish_checksum(udp_checksum);
#endif

    m->l2_len = sizeof(struct ether_hdr);
    m->l3_len = sizeof(struct ipv4_hdr);
    m->ol_flags = PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;

    rte_pktmbuf_append(m, end - rte_pktmbuf_mtod(m, char *));

    return m;
}

struct rte_mbuf *construct_udp_response(unsigned portid, struct rte_mbuf *m)
{
    struct ether_hdr *ether_hdr;
    struct ipv4_hdr *ipv4_hdr;
    struct udp_hdr *udp_hdr;

    struct ether_addr *src_mac;
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t dgram_len;

    char *end = rte_pktmbuf_mtod(m, char *);

    HEADER_PTR(ether_hdr, struct ether_hdr, end);
    HEADER_PTR(ipv4_hdr, struct ipv4_hdr, end);
    HEADER_PTR(udp_hdr, struct udp_hdr, end);

    src_mac = (struct ether_addr *) &ether_hdr->s_addr;
    src_ip = rte_be_to_cpu_32(ipv4_hdr->src_addr);
    src_port = rte_be_to_cpu_16(udp_hdr->src_port);
    dst_port = rte_be_to_cpu_16(udp_hdr->dst_port);
    dgram_len = rte_be_to_cpu_16(udp_hdr->dgram_len) - sizeof(struct udp_hdr);

    return construct_udp_packet(portid, dst_port, src_port,
            src_mac, src_ip, end, dgram_len);
}

#if 0
struct rte_mbuf *construct_udp_response(unsigned portid, struct rte_mbuf *m)
{
    struct rte_mbuf *resp_m = rte_pktmbuf_clone(m, l2fwd_pktmbuf_pool);

    if (resp_m == NULL) {
        return NULL;
    }

    char *end = rte_pktmbuf_mtod(resp_m, char *);

    struct ether_hdr *ether_hdr;
    struct ipv4_hdr *ipv4_hdr;
    struct udp_hdr *udp_hdr;

    HEADER_PTR(ether_hdr, struct ether_hdr, end);
    HEADER_PTR(ipv4_hdr, struct ipv4_hdr, end);
    HEADER_PTR(udp_hdr, struct udp_hdr, end);

    ether_addr_copy(&ether_hdr->s_addr, &ether_hdr->d_addr);
    rte_eth_macaddr_get(portid, &ether_hdr->s_addr);

    ipv4_hdr->hdr_checksum = 0;
    ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
    ipv4_hdr->src_addr = host_ip;

    uint16_t tmp_port = udp_hdr->src_port;
    udp_hdr->src_port = udp_hdr->dst_port;
    udp_hdr->dst_port = tmp_port;
    udp_hdr->dgram_cksum = get_ipv4_psd_sum(ipv4_hdr);

    resp_m->pkt.vlan_macip.f.l2_len = sizeof(struct ether_hdr);
    resp_m->pkt.vlan_macip.f.l3_len = sizeof(struct ipv4_hdr);
    resp_m->ol_flags = PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;

    return resp_m;
}
#endif
