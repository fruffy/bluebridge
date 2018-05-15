#include "dpdk_common.h"
#include "../server_lib.h"
#include "../utils.h"

uint16_t src_server_port = 0;
struct in6_addr src_addr;


//This function is hacky bullshit, needs a lot of improvement.
struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
int rcv_loop(char *rx_buffer, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    while (1) {
        /*
         * Read packet from RX queues
         */
        unsigned nb_rx = rte_eth_rx_burst(0, 0,
                pkts_burst, MAX_PKT_BURST);
        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = pkts_burst[i];
            rte_prefetch0(rte_pktmbuf_mtod(m, void *));
            struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
            struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((char *)eth_hdr + ETH_HDRLEN);
            struct udphdr *udp_hdr = (struct udphdr *)((char *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN);
            uint8_t *payload = ((char *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
            ip6_memaddr *inAddress =  (ip6_memaddr *) &ip_hdr->ip6_dst;
            if (udp_hdr->dest == src_server_port) {
                int isMyID = (inAddress->subid==((ip6_memaddr *)&src_addr.s6_addr)->subid);
                if (isMyID) {
                    rte_memcpy(target_ip->sin6_addr.s6_addr, &ip_hdr->ip6_src, IPV6_SIZE);
                    target_ip->sin6_port = udp_hdr->source;
                    uint16_t msg_size = ntohs(udp_hdr->len) - UDP_HDRLEN;
                    process_request(target_ip, (ip6_memaddr *)&ip_hdr->ip6_dst, payload, msg_size);
                }
            }
            rte_pktmbuf_free(m);
        }
    }
}

void init_server_dpdk(struct config *configstruct) {
    gen_dpdk_packet_info(configstruct);
    config_dpdk();
    src_server_port = configstruct->src_port;
    src_addr = configstruct->src_addr;
}

void enter_dpdk_server_loop(uint16_t server_port) {
    char rx_buffer[BLOCK_SIZE];
    struct sockaddr_in6 *target_ip = rte_malloc(NULL, sizeof(struct sockaddr_in6), 0);
    target_ip->sin6_port = htons(server_port);
    ip6_memaddr remote_addr;
    rcv_loop(rx_buffer, target_ip, &remote_addr);
}