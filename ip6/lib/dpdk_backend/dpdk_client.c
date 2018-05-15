#include "dpdk_common.h"

uint16_t src_client_port = 0;

//This function is hacky bullshit, needs a lot of improvement.
struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
int dpdk_client_rcv(char *receiveBuffer, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr) {
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
            struct ip6_hdr *ip_hdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
            struct udphdr *udp_hdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
            char *payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
            ip6_memaddr *inAddress =  (ip6_memaddr *) &ip_hdr->ip6_dst;
            int isMyID = 1;
            // char s[INET6_ADDRSTRLEN];
            // char s1[INET6_ADDRSTRLEN];
            // inet_ntop(AF_INET6, &ip_hdr->ip6_src, s, sizeof s);
            // inet_ntop(AF_INET6, &ip_hdr->ip6_dst, s1, sizeof s1);
            // printf("Thread %d Got message from %s:%d to %s:%d\n", 0, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
            // printf("Thread %d My port %d their dest port %d\n",0, ntohs(packetinfo.udphdr.source), ntohs(udphdr->dest) );
            if (udp_hdr->dest == src_client_port) {
                // This part in particular is just terrible
                // It is necessary because the IPv6 DPDK filter does not work...
                if (remote_addr != NULL)
                    isMyID = (inAddress->cmd == remote_addr->cmd) && (inAddress->paddr == remote_addr->paddr);
                if (isMyID) {
                    uint16_t msg_size = ntohs(udp_hdr->len) - UDP_HDRLEN;
                    rte_memcpy(receiveBuffer, payload, msg_size);
                    rte_memcpy(target_ip->sin6_addr.s6_addr, &ip_hdr->ip6_src, IPV6_SIZE);
                    target_ip->sin6_port = udp_hdr->source;
                    rte_pktmbuf_free(m);
                    return msg_size;
                }
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