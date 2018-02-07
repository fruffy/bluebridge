#include "checksum.h"
#include <rte_byteorder.h>

static inline uint16_t
get_16b_sum(uint16_t *ptr16, uint32_t nr)
{
    uint32_t sum = 0;
    while (nr > 1)
    {
        sum +=*ptr16;
        nr -= sizeof(uint16_t);
        ptr16++;
        if (sum > UINT16_MAX)
            sum -= UINT16_MAX;
    }

    /* If length is in odd bytes */
    if (nr)
        sum += *((uint8_t*)ptr16);

    sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
    sum &= 0x0ffff;
    return (uint16_t)sum;
}

static uint16_t
get_cksum(void *data, int length)
{
    uint16_t cksum;
    cksum = get_16b_sum((uint16_t*)data, length);
    return (uint16_t)((cksum == 0xffff)?cksum:~cksum);
}

uint16_t
get_ipv4_psd_sum(const struct ipv4_hdr *ip_hdr)
{
    /* Pseudo Header for IPv4/UDP/TCP checksum */
    union ipv4_psd_header {
        struct {
            uint32_t src_addr; /* IP address of source host. */
            uint32_t dst_addr; /* IP address of destination host(s). */
            uint8_t  zero;     /* zero. */
            uint8_t  proto;    /* L4 protocol type. */
            uint16_t len;      /* L4 length. */
        } __attribute__((__packed__));
        uint16_t u16_arr[0];
    } psd_hdr;

    psd_hdr.src_addr = ip_hdr->src_addr;
    psd_hdr.dst_addr = ip_hdr->dst_addr;
    psd_hdr.zero     = 0;
    psd_hdr.proto    = ip_hdr->next_proto_id;
    psd_hdr.len      = rte_cpu_to_be_16((uint16_t)(rte_be_to_cpu_16(ip_hdr->total_length)
                                           - sizeof(struct ipv4_hdr)));
    return get_16b_sum(psd_hdr.u16_arr, sizeof(psd_hdr));
}
