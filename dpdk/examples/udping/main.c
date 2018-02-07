/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>  // for uint8_t
#include <string.h>  // for memset
#include <getopt.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>


#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>


#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_byteorder.h>
#include <rte_pci.h>

#include "main.h"
#include "disassemble.h"
#include "config.h"
#include "generate.h"
#include "rss.h"

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

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

#ifndef THROTTLE_TX
#define MAX_PKT_BURST    32
#else
#define MAX_PKT_BURST    1
#endif
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

static unsigned int l2fwd_rx_queue_per_lcore = 1;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
    unsigned n_rx_port;
    unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

struct mbuf_table {
    unsigned len;
    struct rte_mbuf *m_table[MAX_PKT_BURST];
} __rte_cache_aligned;
struct mbuf_table tx_mbufs[RTE_MAX_LCORE];

/* Per-queue statistics struct */
struct queue_statistics {
    uint64_t tx;
    uint64_t rx;
    uint64_t dropped;
} __rte_cache_aligned;
struct queue_statistics queue_stats[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
    .rxmode = {
        .split_hdr_size = 0,
        .header_split   = 0, /**< Header Split disabled */
        .hw_ip_checksum = 1, /**< IP checksum offload disabled */
        .hw_vlan_filter = 0, /**< VLAN filtering disabled */
        .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
        .hw_strip_crc   = 1, /**< CRC stripped by hardware */
    },
/*    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = ETH_RSS_IPV4 | ETH_RSS_NONFRAG_IPV4_UDP,
        },
    },*/
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

static const struct rte_eth_rxconf rx_conf = {
    .rx_thresh = {
        .pthresh = RX_PTHRESH,
        .hthresh = RX_HTHRESH,
        .wthresh = RX_WTHRESH,
    },
};

static const struct rte_eth_txconf tx_conf = {
    .tx_thresh = {
        .pthresh = TX_PTHRESH,
        .hthresh = TX_HTHRESH,
        .wthresh = TX_WTHRESH,
    },
    .tx_free_thresh = 0, /* Use PMD default values */
    .tx_rs_thresh = 0, /* Use PMD default values */
};

struct rte_mempool * l2fwd_pktmbuf_pool = NULL;

/* Per-port statistics struct */
struct l2fwd_port_statistics {
    uint64_t tx;
    uint64_t rx;
    uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

/* Should we filter out packets not destined for our mac address? (-m) */
int filter_for_this_mac = 0;

/* Send periodic UDP packets? (-s) */
int send_udp_packets = 0;

/* IP Addresses for the two endpoints (-S and -D) */
uint32_t host_ip = 0;
uint32_t dest_ip = 0;
struct ether_addr host_mac;
struct ether_addr dest_mac;

uint64_t rx_ping_count[RTE_MAX_LCORE];

int queues = 0;
uint64_t count = 0;
//rte_spinlock_t count_lock;

/* Send the burst of packets on an output interface */
static int
flush_batched_pkts(struct rte_mbuf **m_table, unsigned n,
        uint8_t port, uint16_t queueid)
{
    unsigned ret;

    ret = rte_eth_tx_burst(port, queueid, m_table, (uint16_t) n);
    queue_stats[queueid].tx += ret;

    if (unlikely(ret < n)) {
        if (ret > 0) {
            int i;

            for (i = 0; i < n - ret; i++) {
                m_table[i] = m_table[ret + i];
            }
        }

#if 0
        queue_stats[queueid].dropped += (n - ret);
        do {
            rte_pktmbuf_free(m_table[ret]);
        } while (++ret < n);
#endif
    }

    return (n - ret);
}

/* Enqueue packets for TX and prepare them to be sent */
static
void batch_packets(uint8_t portid, uint16_t src_port, uint16_t dst_port,
                   char *payload, int payload_size, uint64_t *ping_count)
{
    struct rte_mbuf *m = NULL;
    unsigned lcore_id, len;
    struct mbuf_table *m_table;

    lcore_id = rte_lcore_id();
    m_table = &tx_mbufs[lcore_id];

    len = m_table->len;

    while (len < MAX_PKT_BURST) {

        *(uint64_t *) payload = (*ping_count)++;
        m = construct_udp_packet(portid, src_port, dst_port,
                                 &dest_mac, dest_ip, payload,
                                 payload_size);

        assert (m != NULL);
        m_table->m_table[len++] = m;
    }

    m_table->len = len;
}

static
void send_packet(uint8_t portid, struct rte_mbuf *m)
{
    struct mbuf_table *m_table;
    unsigned lcore_id, len;

    lcore_id = rte_lcore_id();
    m_table = &tx_mbufs[lcore_id];

    len = m_table->len;

    if (len == MAX_PKT_BURST) {
        len = flush_batched_pkts(m_table->m_table, MAX_PKT_BURST,
                                portid, lcore_id);

        if (len == MAX_PKT_BURST) {
            rte_pktmbuf_free(m);
            return;
        }
    }

    m_table->m_table[len++] = m;
    m_table->len = len;
}

#define PRINT_INTERVAL          (5 * 1000 * 1000)       // 5s in ms
// 84 is 64 byte packet+frame gap
#define PKTS_TO_GBPS(p)         ((p * 84 * 8) / (1000 * 1000 * 1000))

/* main processing loop */
static void
l2fwd_main_loop(void)
{
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    struct rte_mbuf *m;
    int sent;
    unsigned lcore_id;
    int queue_id;
    uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
    unsigned i, j, portid, nb_rx;
    struct lcore_queue_conf *qconf;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
            BURST_TX_DRAIN_US;
    struct rte_eth_dev_tx_buffer *buffer;

    prev_tsc = 0;
    timer_tsc = 0;
    uint64_t start_tsc, send_tsc;
    uint16_t src_port;
    uint16_t dst_port;

    /* payload for the udp pings */
    char payload[22] = {0};
    uint64_t udp_ping_count = 1;
    uint64_t last_rx_ping_count = 0;
    uint64_t missed_count = 0;
    uint64_t reorder_count = 0;
    uint64_t ts_hz, print_ts, send_ts;
    uint64_t lcount = 0;

    int is_first = 1;

    lcore_id = rte_lcore_id();
    queue_id = lcore_id;
    qconf = &lcore_queue_conf[lcore_id];

    if (qconf->n_rx_port == 0) {
        RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
        return;
    }

    RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

    for (i = 0; i < qconf->n_rx_port; i++) {

        portid = qconf->rx_port_list[i];
        RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
            portid);

    }
    portid = 0;

    RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

/*    if (send_udp_packets) {
        src_port = SRC_PORT;
        dst_port = DST_PORT + lcore_id;

        while (1) {
            int local_core, remote_core;
            printf("HELLO\n");
            local_core = GetRSSCPUCore(dest_ip, host_ip,
                                    rte_cpu_to_be_16(dst_port),
                                    rte_cpu_to_be_16(src_port),
                                    queues);
            printf("HELL2\n");

            remote_core = GetRSSCPUCore(host_ip, dest_ip,
                                    rte_cpu_to_be_16(src_port),
                                    rte_cpu_to_be_16(dst_port),
                                    queues);
            if (local_core == lcore_id && remote_core == lcore_id) {
                break;
            }

            dst_port++;
        }
    }*/

    ts_hz = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S;
    print_ts = ((uint64_t) PRINT_INTERVAL * ts_hz);
#ifndef THROTTLE_TX
    send_ts = 0;
#else
    send_ts = print_ts / 5;
#endif

    start_tsc = rte_rdtsc();
    prev_tsc = start_tsc;
    send_tsc = start_tsc;

    while (1) {
        uint64_t new_tsc = rte_rdtsc();

        if(send_udp_packets && (new_tsc - send_tsc) > send_ts) {
            batch_packets(0, src_port, dst_port, payload,
                          sizeof(payload), &udp_ping_count);
            send_tsc = rte_rdtsc();
        }

        /*
         * TX burst queue drain
         */
        if (tx_mbufs[lcore_id].len > 0) {
            tx_mbufs[lcore_id].len =
                    flush_batched_pkts(tx_mbufs[lcore_id].m_table,
                    tx_mbufs[lcore_id].len,
                    (uint8_t) portid, queue_id);
        }

        /*
         * Read packet from RX queues
         */
        nb_rx = rte_eth_rx_burst((uint8_t) portid, queue_id,
                pkts_burst, MAX_PKT_BURST);

        for (j = 0; j < nb_rx; j++) {
            uint64_t cur_rx_ping_count;

            m = pkts_burst[j];
            rte_prefetch0(rte_pktmbuf_mtod(m, void *));

            cur_rx_ping_count = rx_ping_count[lcore_id];

            if (ipdump_disassemble_packet(m, portid, filter_for_this_mac) != 0) {
                rte_pktmbuf_free(m);
                continue;
            }

            if (cur_rx_ping_count > last_rx_ping_count) {
                if (cur_rx_ping_count != last_rx_ping_count + 1) {
                    missed_count++;
                    last_rx_ping_count = cur_rx_ping_count;
                }
             } else {
                 reorder_count++;
             }

#if 1
            if(!send_udp_packets) {
                /* echo the ping request */
                struct rte_mbuf *resp_m = construct_udp_response(portid, m);

                assert (resp_m != NULL);

                send_packet(portid, resp_m);
            }
#endif

            rte_pktmbuf_free(m);
#if defined(SHARED_COUNTER)
#ifdef SHARED_LOCK
            rte_spinlock_lock(&count_lock);
#endif
            count++;
/*#ifdef SHARED_LOCK
            rte_spinlock_unlock(&count_lock);
#endif*/
#elif defined(PRIVATE_COUNTER)
            lcount++;
#endif
            queue_stats[queue_id].rx++;
        }

        new_tsc = rte_rdtsc();
        if (new_tsc - prev_tsc >= print_ts) {

            if (unlikely(is_first == 1)) {
                start_tsc = new_tsc;

                queue_stats[lcore_id].tx = 0;
                queue_stats[lcore_id].rx = 0;
                missed_count = 0;
                reorder_count = 0;

                is_first = 0;
            } else {

                double delta_tsc = ((double)(new_tsc - start_tsc)) / (ts_hz * 1000 * 1000);
                double tx_pps = ((double) queue_stats[lcore_id].tx / delta_tsc);
                double rx_pps = ((double) queue_stats[lcore_id].rx / delta_tsc);

#ifndef THROTTLE_TX
                printf("[Core %d] [%0.f s] TX: %.2f (%.0f), RX: %.0f (%.2f), "
#else
                printf("[Core %d] [%0.f s] TX: %ld (%.2f), RX: %ld (%.2f), "
#endif /* THROTTLE_TX */
                        "missed: %ld, reorder: %ld \n",
                        lcore_id, delta_tsc,
#ifndef THROTTLE_TX
                        tx_pps, PKTS_TO_GBPS(tx_pps),
                        rx_pps, PKTS_TO_GBPS(rx_pps),
#else
                        queue_stats[lcore_id].tx, PKTS_TO_GBPS(tx_pps),
                        queue_stats[lcore_id].rx, PKTS_TO_GBPS(rx_pps),
#endif /* THROTTLE_TX */
                        missed_count,
#if defined(SHARED_COUNTER)
                        count);
#elif defined(PRIVATE_COUNTER)
                        lcount);
#else
                        reorder_count);
#endif
            }

            prev_tsc = new_tsc;
        }
    }
}

static int
l2fwd_launch_one_lcore(__attribute__((unused)) void *dummy)
{
    l2fwd_main_loop();
    return 0;
}

/* display usage */
static void
l2fwd_usage(const char *prgname)
{
    printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
            "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
            "  -m : filter out packets destined for other MAC addresses\n"
            "  -s : send periodic UDP packets\n",
            prgname);
}

static int
l2fwd_parse_portmask(const char *portmask)
{
    char *end = NULL;
    unsigned long pm;

    /* parse hexadecimal string */
    pm = strtoul(portmask, &end, 16);
    if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
        return -1;

    if (pm == 0)
        return -1;

    return pm;
}

/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
    int opt, ret;
    char **argvopt;
    int option_index;
    char *prgname = argv[0];
    struct ether_addr tmac = {TAHR_MAC};
    struct ether_addr dmac = {DIONE_MAC};
    static struct option lgopts[] = {
        {NULL, 0, 0, 0}
    };

    argvopt = argv;

    while ((opt = getopt_long(argc, argvopt, "p:msS:D:",
                    lgopts, &option_index)) != EOF) {

        switch (opt) {
            /* portmask */
            case 'p':
                l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
                if (l2fwd_enabled_port_mask == 0) {
                    printf("invalid portmask\n");
                    l2fwd_usage(prgname);
                    return -1;
                }
                break;

                /* filter for this mac address */
            case 'm':
                filter_for_this_mac = 1;
                break;

                /* send UDP packets */
            case 's':
                send_udp_packets = 1;
                break;

                /* src ip specified */
            case 'S':
                ret = inet_pton(AF_INET, optarg, &host_ip);

                if (strcmp(optarg, TAHR_IP) == 0) {
                    ether_addr_copy(&tmac, &host_mac);
                } else if (strcmp(optarg, DIONE_IP) == 0) {
                    ether_addr_copy(&dmac, &host_mac);
                } else {
                    printf("Unknown machine (source)\n");
                    return -1;
                }

                if (ret != 1) {
                    printf("invalid source ip\n");
                    return -1;
                }
                break;

                /* Destination IP specified */
            case 'D':
                ret = inet_pton(AF_INET, optarg, &dest_ip);

                if (strcmp(optarg, TAHR_IP) == 0) {
                    ether_addr_copy(&tmac, &dest_mac);
                } else if (strcmp(optarg, DIONE_IP) == 0) {
                    ether_addr_copy(&dmac, &dest_mac);
                } else {
                    printf("Unknown machine (destination)\n");
                    return -1;
                }

                if (ret != 1) {
                    printf("invalid destination ip\n");
                    return -1;
                }
                break;

                /* long options */
            case 0:
                l2fwd_usage(prgname);
                return -1;

            default:
                l2fwd_usage(prgname);
                return -1;
        }
    }

    if (optind >= 0)
        argv[optind-1] = prgname;

    ret = optind-1;
    optind = 0; /* reset getopt lib */
    return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
    uint8_t portid, count, all_ports_up, print_flag = 0;
    struct rte_eth_link link;

    printf("\nChecking link status");
    fflush(stdout);
    for (count = 0; count <= MAX_CHECK_TIME; count++) {
        all_ports_up = 1;
        for (portid = 0; portid < port_num; portid++) {
            if ((port_mask & (1 << portid)) == 0)
                continue;
            memset(&link, 0, sizeof(link));
            rte_eth_link_get_nowait(portid, &link);
            /* print link status if flag set */
            if (print_flag == 1) {
                if (link.link_status)
                    printf("Port %d Link Up - speed %u "
                            "Mbps - %s\n", (uint8_t)portid,
                            (unsigned)link.link_speed,
                            (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
                            ("full-duplex") : ("half-duplex\n"));
                else
                    printf("Port %d Link Down\n",
                            (uint8_t)portid);
                continue;
            }
            /* clear all_ports_up flag if any link down */
            if (link.link_status == 0) {
                all_ports_up = 0;
                break;
            }
        }
        /* after finally printing all link status, get out */
        if (print_flag == 1)
            break;

        if (all_ports_up == 0) {
            printf(".");
            fflush(stdout);
            rte_delay_ms(CHECK_INTERVAL);
        }

        /* set the print_flag if all ports up or timeout */
        if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
            print_flag = 1;
            printf("done\n");
        }
    }
}

int
MAIN(int argc, char **argv)
{
    struct lcore_queue_conf *qconf;
    struct rte_eth_dev_info dev_info;
    int ret;
    uint8_t nb_ports;
    uint8_t nb_ports_available;
    uint8_t portid, last_port;
    unsigned lcore_id, rx_lcore_id;
    unsigned nb_ports_in_mask = 0;
    int i;

    /* init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    argc -= ret;
    argv += ret;
    /* parse application arguments (after the EAL ones) */
    ret = l2fwd_parse_args(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

    /* create the mbuf pool */
    l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
        MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (l2fwd_pktmbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

    //if (rte_eal_pci_probe() < 0)
    //    rte_exit(EXIT_FAILURE, "Cannot probe PCI\n");

    nb_ports = rte_eth_dev_count();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

    /* reset l2fwd_dst_ports */
    for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
        l2fwd_dst_ports[portid] = 0;
    last_port = 0;

    /*
     * Each logical core is assigned a dedicated TX queue on each port.
     */
    for (portid = 0; portid < nb_ports; portid++) {
        /* skip ports that are not enabled */
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
            continue;

        if (nb_ports_in_mask % 2) {
            l2fwd_dst_ports[portid] = last_port;
            l2fwd_dst_ports[last_port] = portid;
        }
        else
            last_port = portid;

        nb_ports_in_mask++;

        rte_eth_dev_info_get(portid, &dev_info);
    }
    if (nb_ports_in_mask % 2) {
        printf("Notice: odd number of ports in portmask.\n");
        l2fwd_dst_ports[last_port] = last_port;
    }

    rx_lcore_id = 0;
    qconf = NULL;

    /* Initialize the port/queue configuration of each logical core */
    for (portid = 0; portid < nb_ports; portid++) {
        /* skip ports that are not enabled */
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
            continue;

        /* get the lcore_id for this port */
        while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
               lcore_queue_conf[rx_lcore_id].n_rx_port ==
               l2fwd_rx_queue_per_lcore) {
            rx_lcore_id++;
            if (rx_lcore_id >= RTE_MAX_LCORE)
                rte_exit(EXIT_FAILURE, "Not enough cores\n");
        }

        if (qconf != &lcore_queue_conf[rx_lcore_id])
            /* Assigned a new logical core in the loop above. */
            qconf = &lcore_queue_conf[rx_lcore_id];

        qconf->rx_port_list[qconf->n_rx_port] = portid;
        qconf->n_rx_port++;
        printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
    }

    nb_ports_available = nb_ports;

    memset(rx_ping_count, 0, RTE_MAX_LCORE * sizeof(uint64_t));

    //toeplitz_init(NULL);

    //rte_spinlock_init(&count_lock);

    /* Initialise each port */
    for (portid = 0; portid < nb_ports; portid++) {
        /* skip ports that are not enabled */
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
            printf("Skipping disabled port %u\n", (unsigned) portid);
            nb_ports_available--;
            continue;
        }
        /* init port */
        printf("Initializing port %u... ", (unsigned) portid);
        fflush(stdout);
        ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);

        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
                  ret, (unsigned) portid);

        ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
                               &nb_txd);
        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                 "Cannot adjust number of descriptors: err=%d, port=%u\n",
                 ret, (unsigned) portid);

        rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

        /* init one RX queue */
        fflush(stdout);
        ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
                         rte_eth_dev_socket_id(portid),
                         NULL,
                         l2fwd_pktmbuf_pool);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
                  ret, (unsigned) portid);

        /* init one TX queue on each port */
        fflush(stdout);
        ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
                rte_eth_dev_socket_id(portid),
                NULL);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
                ret, (unsigned) portid);

        /* Initialize TX buffers */
        tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
                RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
                rte_eth_dev_socket_id(portid));
        if (tx_buffer[portid] == NULL)
            rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                    (unsigned) portid);

        rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

        ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
                rte_eth_tx_buffer_count_callback,
                &port_statistics[portid].dropped);
        if (ret < 0)
                rte_exit(EXIT_FAILURE, "Cannot set error callback for "
                        "tx buffer on port %u\n", (unsigned) portid);

        /* Start device */
        ret = rte_eth_dev_start(portid);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
                  ret, (unsigned) portid);

        printf("done: \n");

        rte_eth_promiscuous_enable(portid);

        printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
                (unsigned) portid,
                l2fwd_ports_eth_addr[portid].addr_bytes[0],
                l2fwd_ports_eth_addr[portid].addr_bytes[1],
                l2fwd_ports_eth_addr[portid].addr_bytes[2],
                l2fwd_ports_eth_addr[portid].addr_bytes[3],
                l2fwd_ports_eth_addr[portid].addr_bytes[4],
                l2fwd_ports_eth_addr[portid].addr_bytes[5]);

        /* initialize port stats */
        memset(&port_statistics, 0, sizeof(port_statistics));
    }

    if (!nb_ports_available) {
        rte_exit(EXIT_FAILURE,
            "All available ports are disabled. Please set portmask.\n");
    }

    check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

    ret = 0;
    /* launch per-lcore init on every lcore */
    rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MASTER);
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            ret = -1;
            break;
        }
    }

    for (portid = 0; portid < nb_ports; portid++) {
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
            continue;
        printf("Closing port %d...", portid);
        rte_eth_dev_stop(portid);
        rte_eth_dev_close(portid);
        printf(" Done\n");
    }
    printf("Bye...\n");

    return ret;
}

