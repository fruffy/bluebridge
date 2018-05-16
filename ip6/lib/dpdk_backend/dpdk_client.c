#include "dpdk_common.h"

uint16_t src_client_port = 0;

//This function is hacky bullshit, needs a lot of improvement.
struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
int dpdk_client_rcv(uint8_t *rcv_buf, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
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
            struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((uint8_t *)eth_hdr + ETH_HDRLEN);
            struct udphdr *udp_hdr = (struct udphdr *)((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN);
            uint8_t *payload = ((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
            //ip6_memaddr *inAddress =  (ip6_memaddr *) &ip_hdr->ip6_dst;
            //int isMyID = 1;
            // This should be debug code...
            // printf("Thread %d Message from ", thread_id), print_ip_addr(&ip_hdr->ip6_src);
            // printf("_%d to ", ntohs(udp_hdr->source)), print_ip_addr(&ip_hdr->ip6_dst), printf("_%d\n", ntohs(udp_hdr->dest))
            if (udp_hdr->dest == src_client_port) {
                // This part in particular is just terrible
                // It is necessary because the IPv6 DPDK filter does not work...
                // if (remote_addr != NULL)
                //     isMyID = (inAddress->cmd == remote_addr->cmd) && (inAddress->paddr == remote_addr->paddr);
                uint16_t msg_size = ntohs(udp_hdr->len) - UDP_HDRLEN;
                if (rcv_buf != NULL)
                    rte_memcpy(rcv_buf, payload, msg_size);
                if (remote_addr != NULL)
                    rte_memcpy(remote_addr, &ip_hdr->ip6_dst, IPV6_SIZE);
                rte_memcpy(target_ip->sin6_addr.s6_addr, &ip_hdr->ip6_src, IPV6_SIZE);
                target_ip->sin6_port = udp_hdr->source;
                rte_pktmbuf_free(m);
                return msg_size;
            }
            rte_pktmbuf_free(m);
        }
    }
}

//This function is hacky bullshit, needs a lot of improvement.
void write_dpdk_packets(int num_packets) {
    int packets = 0;
    while (1){
        if (packets == num_packets)
            break;
        unsigned nb_rx = rte_eth_rx_burst(0, 0,
                pkts_burst, MAX_PKT_BURST);
        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = pkts_burst[i];
            rte_prefetch0(rte_pktmbuf_mtod(m, void *));
            struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
            struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((uint8_t *)eth_hdr + ETH_HDRLEN);
            struct udphdr *udp_hdr = (struct udphdr *)((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN);
            uint8_t *payload = ((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
            ip6_memaddr *inAddress =  (ip6_memaddr *) &ip_hdr->ip6_dst;
            int isMyID = 1;
            // This should be debug code...
            // printf("Thread %d Message from ", thread_id), print_ip_addr(&ip_hdr->ip6_src);
            // printf("_%d to ", ntohs(udp_hdr->source)), print_ip_addr(&ip_hdr->ip6_dst), printf("_%d\n", ntohs(udp_hdr->dest));
            if (udp_hdr->dest == src_client_port) {
                uint8_t *payload = ((uint8_t *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
                ip6_memaddr *inAddress =  (ip6_memaddr *) &ip_hdr->ip6_dst;
                uint16_t msg_size = ntohs(udp_hdr->len) - UDP_HDRLEN;
                memcpy((void *) (inAddress->paddr), payload, msg_size);
                packets++;
            }
            rte_pktmbuf_free(m);
        }
    }
}



void init_client_dpdk(struct config *configstruct) {
    src_client_port = configstruct->src_port;
    gen_dpdk_packet_info(configstruct);
    config_dpdk();
}