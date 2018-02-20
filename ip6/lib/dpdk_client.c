#include "dpdkcommon.h"

uint16_t src_client_port = 0;

//This function is hacky bullshit, needs a lot of improvement.
struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
int dpdk_client_rcv(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int server) {
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
            // char s[INET6_ADDRSTRLEN];
            // char s1[INET6_ADDRSTRLEN];
            // inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
            // inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
            // printf("Thread %d Got message from %s:%d to %s:%d\n", 0, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
            // printf("Thread %d My port %d their dest port %d\n",0, ntohs(packetinfo.udphdr.source), ntohs(udphdr->dest) );
            if (udphdr->dest == src_client_port) {
                // This part in particular is just terrible
                // It is necessary because the IPv6 DPDK filter does not work...
                if (remoteAddr != NULL)
                    isMyID = (inAddress->cmd == remoteAddr->cmd) && (inAddress->paddr == remoteAddr->paddr);
                if (isMyID) {
                    rte_memcpy(receiveBuffer, payload, msgBlockSize);
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


void init_client_dpdk(struct config *configstruct) {
    src_client_port = configstruct->src_port;
    gen_dpdk_packet_info(configstruct);
    config_dpdk();
}