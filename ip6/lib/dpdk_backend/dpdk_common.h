#ifndef BB_DPDK_COMMON
#define BB_DPDK_COMMON
#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close(), sleep()
#include <string.h>           // strcpy, memset(), and rte_memcpy()
#include <netinet/udp.h>      // struct udphdr
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <errno.h>            // errno, perror()
#include <stdint.h>           // for uint8_t
#include <arpa/inet.h>        // inet_pton() and inet_ntop()


#include <rte_cycles.h>       // rte_delay_ms
#include <rte_flow.h>
#include <rte_ethdev.h>       // main DPDK library
#include <rte_malloc.h>       // rte_zmalloc_socket()

#include "../config.h"
#include "../types.h"


#define MTU 8192
// Hardcoded source and dest macs
static const uint8_t SRC_MAC[6] = { 0xa0, 0x36, 0x9f, 0x45, 0xd8, 0x74 };
static const uint8_t DST_MAC[6] = { 0xa0, 0x36, 0x9f, 0x45, 0xd8, 0x75 };


#define RTE_LOGTYPE_bb RTE_LOGTYPE_USER1

#define MBUF_SIZE (256 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF   8192
#define MEMPOOL_CACHE_SIZE 256
#define MBUF_CACHE_SIZE 256

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define MAX_PKT_BURST    100
#define BURST_TX_DRAIN_US 5 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr bb_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t bb_enabled_port_mask = 0;

/* list of enabled ports */
static uint32_t bb_dst_ports[RTE_MAX_ETHPORTS];

static unsigned int bb_rx_queue_per_lcore = 1;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
    unsigned n_rx_port;
    unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];
static struct rte_mempool *bb_pktmbuf_pool = NULL;

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

struct mbuf_table {
    unsigned len;
    struct rte_mbuf *m_table[MAX_PKT_BURST];
} __rte_cache_aligned;
struct mbuf_table tx_mbufs[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
    .rxmode = {
        .split_hdr_size = 0,
        .header_split   = 0, /**< Header Split disabled */
        .hw_ip_checksum = 0, /**< IP checksum offload disabled */
        .hw_vlan_filter = 0, /**< VLAN filtering disabled */
        .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
        .hw_strip_crc   = 0, /**< CRC stripped by hardware */
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = ETH_RSS_IP | ETH_RSS_UDP,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

#define HEADER_PTR(var, type, end) \
    var = (type *)end, end += sizeof(type)

#endif

extern int config_dpdk();
extern struct p_skeleton *gen_dpdk_packet_info();
extern void enter_dpdk_server_loop(uint16_t server_port);
extern void init_client_dpdk(struct config *configstruct);
extern void init_server_dpdk(struct config *configstruct);
extern int dpdk_send(pkt_rqst pkt);
extern void write_dpdk_packets(int num_packets);
extern void dpdk_batched_send(pkt_rqst *pkts, int num_pkts, uint32_t *sub_ids);
extern int dpdk_server_rcv(uint8_t *rx_buffer, int msg_size, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr);
extern int dpdk_client_rcv(uint8_t *rx_buffer, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr);

#define RCV(rcv_buf, target_ip, remote_addr) dpdk_client_rcv(rcv_buf, target_ip, remote_addr)
#define SEND(pkt) dpdk_send(pkt)
#define RCV_BULK(num_packets) write_dpdk_packets(num_packets)
#define SEND_BATCHED(pkts, num_addrs, sub_ids) dpdk_batched_send(pkts, num_addrs, sub_ids)
