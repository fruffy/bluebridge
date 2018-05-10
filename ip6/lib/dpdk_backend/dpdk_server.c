#include "dpdk_common.h"
#include "../server_lib.h"
#include "../utils.h"

uint16_t src_server_port = 0;
struct in6_addr src_addr;


//This function is hacky bullshit, needs a lot of improvement.
struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
int dpdk_server_rcv(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, ip6_memaddr *remoteAddr) {
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
            ip6_memaddr *inAddress =  (ip6_memaddr *) &iphdr->ip6_dst;
            int isMyID = 1;
            // char s[INET6_ADDRSTRLEN];
            // char s1[INET6_ADDRSTRLEN];
            // inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
            // inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
            // printf("Thread %d Got message from %s:%d to %s:%d\n", 0, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
            // printf("Thread %d My port %d their dest port %d\n",0, ntohs(packetinfo.udphdr.source), ntohs(udphdr->dest) );
            if (udphdr->dest == src_server_port) {
                isMyID = (inAddress->subid == ((ip6_memaddr *)&src_addr.s6_addr)->subid);
                // printf("Thread %d Their ID\n", 0);
                // print_n_bytes(inAddress, 16);
                // printf("Thread %d MY ID\n", 0);
                // print_n_bytes(&src_addr.s6_addr, 16);
                if (isMyID) {
                    rte_memcpy(receiveBuffer, payload, msgBlockSize);
                    if (remoteAddr != NULL) {
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

//This function is hacky bullshit, needs a lot of improvement.
int rcv_loop(char *receiveBuffer, struct sockaddr_in6 *targetIP, ip6_memaddr *remoteAddr) {
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
            char *payload = ((char *)eth_hdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
            ip6_memaddr *inAddress =  (ip6_memaddr *) &ip_hdr->ip6_dst;
            if (udp_hdr->dest == src_server_port) {
                int isMyID = (inAddress->subid==((ip6_memaddr *)&src_addr.s6_addr)->subid);
                if (isMyID) {
                    rte_memcpy(targetIP->sin6_addr.s6_addr, &ip_hdr->ip6_src, IPV6_SIZE);
                    targetIP->sin6_port = udp_hdr->source;
                    process_request(payload, targetIP, (ip6_memaddr *)&ip_hdr->ip6_dst);
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
    char receiveBuffer[BLOCK_SIZE];
    struct sockaddr_in6 *targetIP = rte_malloc(NULL, sizeof(struct sockaddr_in6), 0);
    targetIP->sin6_port = htons(server_port);
    ip6_memaddr remoteAddr;
    rcv_loop(receiveBuffer, targetIP, &remoteAddr);
}