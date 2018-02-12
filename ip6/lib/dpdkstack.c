
#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close(), sleep()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netinet/udp.h>      // struct udphdr
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <errno.h>            // errno, perror()
#include <stdint.h>           // for uint8_t
#include <arpa/inet.h>        // inet_pton() and inet_ntop()


#include <rte_ethdev.h>       // main DPDK library
#include <rte_malloc.h>       // rte_zmalloc_socket()
#include <rte_cycles.h>       // rte_delay_ms
#include <rte_flow.h>


#include "udpcooked.h"
#include "utils.h"
#include "config.h"


#define MTU 8192

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

#ifndef THROTTLE_TX
#define MAX_PKT_BURST    32
#else
#define MAX_PKT_BURST    1
#endif
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
struct rte_mempool * bb_pktmbuf_pool = NULL;

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

struct packetconfig {
    struct ip6_hdr iphdr;
    struct udphdr udphdr;
    struct sockaddr_ll device;
    unsigned char ether_frame[IP_MAXPACKET];
};

static struct packetconfig packetinfo;
static char *ring;

static int send_packet(uint8_t portid, struct rte_mbuf *m) {
    int b_sent = 0, sent = 0;
    b_sent = rte_eth_tx_buffer(0,0,tx_buffer[0], m);
    sent = rte_eth_tx_buffer_flush(0,0,tx_buffer[0]);
    return sent;
}

// datalen = 4158
int send_dpdk_packet(char *data, int datalen) {
    struct rte_mbuf *m = rte_pktmbuf_alloc(bb_pktmbuf_pool);
    if (m == NULL) {
        return EXIT_FAILURE;
    }
    char *end = rte_pktmbuf_mtod(m, char *);
    rte_memcpy(end, data, datalen);
    end += datalen;
    rte_pktmbuf_append(m, end - rte_pktmbuf_mtod(m, char *));
    send_packet(0, m);
    //rte_pktmbuf_free(m);
    return EXIT_SUCCESS;
}

int dpdk_send(struct pkt_rqst pkt) {
    struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)packetinfo.ether_frame + ETH_HDRLEN);
    struct udphdr *udphdr = (struct udphdr *)((char *)packetinfo.ether_frame + ETH_HDRLEN + IP6_HDRLEN);
    //Set destination IP
    iphdr->ip6_dst = *pkt.dst_addr;
    // Payload length (16 bits): UDP header + UDP data
    iphdr->ip6_plen = htons (UDP_HDRLEN + pkt.datalen);
    // UDP header
    // Destination port number (16 bits): pick a number
    // We expect the port to already be in network byte order
    udphdr->dest = pkt.dst_port;
    // Length of UDP datagram (16 bits): UDP header + UDP data
    udphdr->len =  UDP_HDRLEN + pkt.datalen;
    // Static UDP checksum
    udphdr->check = 0xFFAA;
    //udphdr->check = udp6_checksum (iphdr, udphdr, (uint8_t *) data, datalen);
    // Copy our data
    memcpy(packetinfo.ether_frame + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN, pkt.data, pkt.datalen * sizeof (uint8_t));
    // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + UDP header + UDP data)
    int frame_length = 6 + 6 + 2 + IP6_HDRLEN + UDP_HDRLEN + pkt.datalen; // Usually 4158
/*    char dst_ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6,&iphdr->ip6_dst, dst_ip, sizeof dst_ip);
    printf("Sending to part two %s::%d\n", dst_ip, ntohs(udphdr->dest));*/
    // Commit the dpdk packet
    send_dpdk_packet(packetinfo.ether_frame, frame_length);
    return EXIT_SUCCESS;
}


struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
int dpdk_rcv(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int server) {
    while (1) {
        /*
         * Read packet from RX queues
         */
        unsigned nb_rx = rte_eth_rx_burst(0, 0,
                pkts_burst, MAX_PKT_BURST);
        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = pkts_burst[i];
            rte_prefetch0(rte_pktmbuf_mtod(m, void *));
            struct ether_hdr *ethhdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
            struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
            struct udphdr *udphdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
            char *payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
            struct in6_memaddr *inAddress =  (struct in6_memaddr *) &iphdr->ip6_dst;
            int isMyID = 1;
/*            char s[INET6_ADDRSTRLEN];
            char s1[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
            inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
            printf("Thread %d Got message from %s:%d to %s:%d\n", 0, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
            printf("Thread %d My port %d their dest port %d\n",0, ntohs(packetinfo.udphdr.source), ntohs(udphdr->dest) );*/
            if (udphdr->dest == packetinfo.udphdr.source) {
                if (remoteAddr != NULL && !server) {
/*                    printf("Thread %d Their ID\n", 0);
                    print_n_bytes(inAddress, 16);
                    printf("Thread %d MY ID\n", 0);
                    print_n_bytes(remoteAddr, 16);*/
                    isMyID = (inAddress->cmd == remoteAddr->cmd) && (inAddress->paddr == remoteAddr->paddr);
                }
                if (isMyID) {
                    rte_memcpy(receiveBuffer, payload, msgBlockSize);
                    if (remoteAddr != NULL && server) {
                        rte_memcpy(remoteAddr, &iphdr->ip6_dst, IPV6_SIZE);
                    }
                    rte_memcpy(targetIP->sin6_addr.s6_addr, &iphdr->ip6_src, IPV6_SIZE);
                    targetIP->sin6_port = udphdr->source;
                    rte_pktmbuf_free(m);
                    return udphdr->len - UDP_HDRLEN;
                }
            }
            rte_pktmbuf_free(m);
        }
    }
}

struct packetconfig *gen_dpdk_packet_info(struct config *configstruct) {

    // Allocate memory for various arrays.

    // Find interface index from interface name and store index in
    // struct sockaddr_ll device, which will be used as an argument of sendto().
    memset (&packetinfo.device, 0, sizeof (packetinfo.device));
    if ((packetinfo.device.sll_ifindex = if_nametoindex (configstruct->interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
    //    exit (EXIT_FAILURE);
    }
    //memcpy(src_mac, packetinfo.device.sll_addr, 6); 
    uint8_t src_mac[6] = { 0xa0, 0x36, 0x9f, 0x45, 0xd8, 0x74 };
    uint8_t dst_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    // Fill out sockaddr_ll.
    packetinfo.device.sll_family = AF_PACKET;
    memcpy (packetinfo.device.sll_addr, src_mac, 6 * sizeof (uint8_t));
    packetinfo.device.sll_halen = 6;

    // Destination and Source MAC addresses
    memcpy (packetinfo.ether_frame, &dst_mac, 6 * sizeof (uint8_t));
    memcpy (packetinfo.ether_frame + 6, &src_mac, 6 * sizeof (uint8_t));
    // Next is ethernet type code (ETH_P_IPV6 for IPv6).
    // http://www.iana.org/assignments/ethernet-numbers
    packetinfo.ether_frame[12] = ETH_P_IPV6 / 256;
    packetinfo.ether_frame[13] = ETH_P_IPV6 % 256;

    // IPv6 header
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    packetinfo.iphdr.ip6_src = configstruct->src_addr;
    // IPv6 version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    packetinfo.iphdr.ip6_flow = htonl ((6 << 28) | (0 << 20) | 0);

    // Next header (8 bits): 17 for UDP
    packetinfo.iphdr.ip6_nxt = IPPROTO_UDP;
    //packetinfo.iphdr.ip6_nxt = 0x9F;

    // Hop limit (8 bits): default to maximum value
    packetinfo.iphdr.ip6_hops = 255;
    packetinfo.udphdr.source = configstruct->src_port;
    memcpy (packetinfo.ether_frame + ETH_HDRLEN, &packetinfo.iphdr, IP6_HDRLEN * sizeof (uint8_t));
    memcpy (packetinfo.ether_frame + ETH_HDRLEN + IP6_HDRLEN, &packetinfo.udphdr, UDP_HDRLEN * sizeof (uint8_t));
    return &packetinfo;
}

static int bb_parse_portmask(const char *portmask)
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
bb_parse_args(int argc, char **argvopt) {
    bb_enabled_port_mask = bb_parse_portmask("ffff");
    if (bb_enabled_port_mask == 0) {
        printf("invalid portmask\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/* Check the link status of all ports in up to 9s, and print them finally */
static void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask) {
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

unsigned config_ports(uint8_t portid, uint8_t nb_ports, unsigned rx_lcore_id) {

    /* Initialize the port/queue configuration of each logical core */
    
    unsigned int nb_lcores = 0;
    struct lcore_queue_conf *qconf = NULL;
    for (portid = 0; portid < nb_ports; portid++) {
        /* skip ports that are not enabled */
        if ((bb_enabled_port_mask & (1 << portid)) == 0)
            continue;

        /* get the lcore_id for this port */
        while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
               lcore_queue_conf[rx_lcore_id].n_rx_port ==
               bb_rx_queue_per_lcore) {
            rx_lcore_id++;
            if (rx_lcore_id >= RTE_MAX_LCORE)
                rte_exit(EXIT_FAILURE, "Not enough cores\n");
        }

        if (qconf != &lcore_queue_conf[rx_lcore_id]) {
            /* Assigned a new logical core in the loop above. */
            qconf = &lcore_queue_conf[rx_lcore_id];
            nb_lcores++;
        }

        qconf->rx_port_list[qconf->n_rx_port] = portid;
        qconf->n_rx_port++;
        printf("Lcore %u: RX port %u\n", rx_lcore_id, portid);
    }
    return nb_lcores;
}


int init_ports(uint8_t portid, uint8_t nb_ports) {
    uint8_t nb_ports_available = nb_ports;
    int ret;
    for (portid = 0; portid < nb_ports; portid++) {
        struct rte_eth_rxconf rxq_conf;
        struct rte_eth_txconf txq_conf;
        struct rte_eth_conf local_port_conf = port_conf;
        struct rte_eth_dev_info dev_info;

        /* skip ports that are not enabled */
        if ((bb_enabled_port_mask & (1 << portid)) == 0) {
            printf("Skipping disabled port %u\n", portid);
            nb_ports_available--;
            continue;
        }

        /* init port */
        printf("Initializing port %u... ", portid);
        fflush(stdout);
        rte_eth_dev_info_get(portid, &dev_info);
        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
            local_port_conf.txmode.offloads |=
                DEV_TX_OFFLOAD_MBUF_FAST_FREE;
        ret = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
                  ret, portid);

        ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
                               &nb_txd);
        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                 "Cannot adjust number of descriptors: err=%d, port=%u\n",
                 ret, portid);

        rte_eth_macaddr_get(portid,&bb_ports_eth_addr[portid]);
        
        rte_eth_dev_set_mtu (portid, MTU);

        /* init one RX queue */
        fflush(stdout);
        rxq_conf = dev_info.default_rxconf;
        //rxq_conf.offloads = local_port_conf.rxmode.offloads;
        rxq_conf.rx_drop_en = 1;
        ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
                         rte_eth_dev_socket_id(portid),
                         NULL,
                         bb_pktmbuf_pool);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
                  ret, portid);

        /* init one TX queue on each port */
        fflush(stdout);
        txq_conf = dev_info.default_txconf;
        txq_conf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
        txq_conf.offloads = local_port_conf.txmode.offloads;

        ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
                rte_eth_dev_socket_id(portid),
                NULL);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
                ret, portid);

        /* Initialize TX buffers */
        tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
                RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
                rte_eth_dev_socket_id(portid));
        if (tx_buffer[portid] == NULL)
            rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                    portid);

        rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

        /* Start device */
        ret = rte_eth_dev_start(portid);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
                  ret, portid);
        printf("done: \n");
        rte_eth_promiscuous_enable(portid);
    }
    return ret;
}


unsigned setup_ports (uint8_t portid, uint8_t nb_ports) {
    unsigned nb_ports_in_mask = 0;
    struct rte_eth_dev_info dev_info;

    /* reset bb_dst_ports */
    for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
        bb_dst_ports[portid] = 0;
    uint8_t last_port = 0;
    /*
     * Each logical core is assigned a dedicated TX queue on each port.
     */
    for (portid = 0; portid < nb_ports; portid++) {
        /* skip ports that are not enabled */
        if ((bb_enabled_port_mask & (1 << portid)) == 0)
            continue;

        if (nb_ports_in_mask % 2) {
            bb_dst_ports[portid] = last_port;
            bb_dst_ports[last_port] = portid;
        }
        else
            last_port = portid;

        nb_ports_in_mask++;

        rte_eth_dev_info_get(portid, &dev_info);
    }
    if (nb_ports_in_mask % 2) {
        printf("Notice: odd number of ports in portmask.\n");
        bb_dst_ports[last_port] = last_port;
    }
    return nb_ports_in_mask;
}

struct rte_flow *generate_ipv6_flow(uint8_t port_id, uint16_t rx_q, struct in6_addr dest_ip, struct rte_flow_error *error) {
    struct rte_flow_attr attr;
    struct rte_flow_item pattern[3];
    struct rte_flow_action action[3];
    struct rte_flow *flow = NULL;
    struct rte_flow_action_queue queue = { .index = rx_q };
    struct rte_flow_item_udp udp_spec;
    struct rte_flow_item_udp udp_mask;
    struct rte_flow_item_ipv6 ip_spec;
    struct rte_flow_item_ipv6 ip_mask;
    int res;

    memset(pattern, 0, sizeof(pattern));
    memset(action, 0, sizeof(action));
    char dst_ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6,&packetinfo.iphdr.ip6_src, dst_ip, sizeof dst_ip);
    printf("Sending to part two %s\n", dst_ip);


    //char subnet_mask[INET6_ADDRSTRLEN];
    //inet_ntop(AF_INET6, &packetinfo.iphdr.ip6_src, subnet_mask, sizeof subnet_mask);
    unsigned char subnet_mask[sizeof(struct in6_addr)];
    inet_pton(AF_INET6, dst_ip, subnet_mask);
    memcpy(&subnet_mask[8], "\xff\xff\xff\xff\xff\xff\xff\xff", 8);
    printf("%s\n",subnet_mask );
    /*
     * set the rule attribute.
     * in this case only ingress packets will be checked.
     */
    memset(&attr, 0, sizeof(struct rte_flow_attr));
    attr.ingress = 1;

    /*
     * create the action sequence.
     * one action only,  move packet to queue
     */

    action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    action[0].conf = &queue;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;

    /*
     * setting the third level of the pattern (ip).
     * in this example this is the level we care about
     * so we set it according to the parameters.
     */
    memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv6));
    memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv6));
    memcpy(ip_spec.hdr.dst_addr, &packetinfo.iphdr.ip6_src , IPV6_SIZE);
    //memcpy(ip_mask.hdr.dst_addr, subnet_mask, IPV6_SIZE);

    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
    pattern[0].spec = &ip_spec;
    pattern[0].mask = &ip_mask;

    udp_spec.hdr.src_port = packetinfo.udphdr.source;
    udp_mask.hdr.src_port = 65535;


    pattern[1].type = RTE_FLOW_ITEM_TYPE_UDP;
    pattern[1].spec = &udp_spec;
    pattern[1].mask = &udp_mask;

    /* the final level must be always type end */
    pattern[2].type = RTE_FLOW_ITEM_TYPE_END;

    res = rte_flow_validate(port_id, &attr, pattern, action, error);
    if (!res)
        flow = rte_flow_create(port_id, &attr, pattern, action, error);
    return flow;
}


int config_dpdk() {
    struct lcore_queue_conf *qconf;
    int ret;
    uint8_t nb_ports;
    uint8_t portid, last_port;
    unsigned lcore_id, rx_lcore_id;
    unsigned int nb_mbufs;
    char *sample_input[] = {"bluebridge", "-l", "0","-n","1", "--", "-p", "ffff", NULL};
    char **argv = sample_input;
    int argc = sizeof(sample_input) / sizeof(char*) - 1;

    printf("%d argc\n", argc);
    
    /* init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    argc -= ret;
    argv += ret;

    /* parse application arguments (after the EAL ones) */
    ret = bb_parse_args(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid bb arguments\n");


    //if (rte_eal_pci_probe() < 0)
    //    rte_exit(EXIT_FAILURE, "Cannot probe PCI\n");

    nb_ports = rte_eth_dev_count();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

    rx_lcore_id = 0;
    portid = 0;
    unsigned nb_ports_in_mask = setup_ports(portid ,nb_ports);


    /* Initialize the port/queue configuration of each logical core */
    unsigned int nb_lcores = config_ports(portid , nb_ports, rx_lcore_id);


    nb_mbufs = RTE_MAX(nb_ports * (nb_rxd + nb_txd + MAX_PKT_BURST +
        nb_lcores * MEMPOOL_CACHE_SIZE), 8192U);
    /* create the mbuf pool */
    bb_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
        MEMPOOL_CACHE_SIZE, 0, MTU, rte_socket_id());
    if (bb_pktmbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

    /* Initialise each port */
    ret = init_ports(portid, nb_ports);

    check_all_ports_link_status(nb_ports, bb_enabled_port_mask);

    printf("Waiting for the interface to initialise...\n");
    sleep(2);
    printf("NIC should be ready now...\n");

    /* create flow for send packet with */
    // struct rte_flow_error error;
    // struct rte_flow *flow = generate_ipv6_flow(portid, rx_lcore_id, packetinfo.iphdr.ip6_dst, &error);
    // if (!flow) {
    //     printf("Flow can't be created %d message: %s\n",
    //         error.type,
    //         error.message ? error.message : "(no stated reason)");
    //     rte_exit(EXIT_FAILURE, "error in creating flow");
    // }

    return ret;
}

struct packetconfig *get_dpdk_packet_info() {
    return &packetinfo;
}

void init_dpdk(struct config *configstruct) {
    gen_dpdk_packet_info(configstruct);
    config_dpdk();
}