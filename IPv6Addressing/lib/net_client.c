#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <features.h>       /* for the glibc version number */
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h> /* The L2 protocols */
#include <string.h>
#include <netinet/in.h>
#include <stdint.h>
#include <ctype.h>
#include <netinet/udp.h>      // struct udphdr


#include "udpcooked.h"
#include "utils.h"
#include "config.h"

//#include <asm/system.h>
typedef struct _rxring *rxring_t;

typedef int (*rx_cb_t)(void *u, const uint8_t *buf, size_t len);

rxring_t rxring_init();
int rxring_fanout_hash(rxring_t rx, uint16_t id);

struct priv {
    int test;
    /* unused */
};

struct _rxring {
    void *user;
    rx_cb_t cb;
    uint8_t *map;
    size_t map_sz;
    sig_atomic_t cancel;
    unsigned int r_idx;
    unsigned int nr_blocks;
    unsigned int block_sz;
    int ifindex;
    int fd;
};

#define NUM_BLOCKS 2049

struct sockaddr_ll sll;
int client_my_port;
static rxring_t rx_global;
struct pollfd pfd;

const char *ifname;
/* Initialize a listening socket */
struct sockaddr_in6 *rx_client_init_socket(struct config *configstruct) {
    struct sockaddr_in6 *temp = malloc(sizeof(struct sockaddr_in6));
    client_my_port = configstruct->src_port;
    memset (&sll, 0, sizeof (sll));
    if ((sll.sll_ifindex = if_nametoindex (configstruct->interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons (ETH_P_ALL);
    rx_global = rxring_init();
    if ( NULL == rx_global ){
        free(temp);
        return NULL;
    }

    if ( !rxring_fanout_hash(rx_global, 0x1234) ) {
        free(temp);
        return NULL;
    }

    pfd.fd = rx_global->fd;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;
    //rxring_mainloop(rx);
    return temp;
}

int client_get_rcv_socket() {
    return rx_global->fd;
}

void client_close_rcv_socket() {
    close(rx_global->fd);
}

/* 1. Open the packet socket */
static int packet_socket(rxring_t rx)
{
    if ((rx->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        perror("socket()");
        return 0;
    }

    return 1;
}

/* 2. Set TPACKET_V3 */
static int set_v3(rxring_t rx)
{
    int val = TPACKET_V3;
    if (setsockopt(rx->fd, SOL_PACKET, PACKET_VERSION, &val, sizeof(val))) {
        perror("setsockopt(TPACKET_V3)");
        return 0;
    };

    return 1;
}

/* 3. Setup the fd for mmap() ring buffer */
static int rx_ring(rxring_t rx)
{
    struct tpacket_req3 req;

/*    req.tp_block_size = getpagesize() << 2;
    req.tp_block_nr = NUM_BLOCKS;
    req.tp_frame_size = TPACKET_ALIGNMENT << 7;
    req.tp_frame_nr = req.tp_block_size /
                req.tp_frame_size *
                req.tp_block_nr;*/
    req.tp_block_size = BLOCKSIZE,
    req.tp_block_nr = 1,
    req.tp_frame_size = FRAMESIZE,
    req.tp_frame_nr = CONF_RING_FRAMES,
    req.tp_retire_blk_tov = 64;
    req.tp_sizeof_priv = sizeof(struct priv);
    req.tp_feature_req_word = 0;
    //req.tp_feature_req_word |= TP_REQ_FILL_RXHASH;
    if (setsockopt(rx->fd, SOL_PACKET, PACKET_RX_RING,
            (char *)&req, sizeof(req))) {
        perror("setsockopt(PACKET_RX_RING)");
        return 0;
    };

    rx->map_sz = req.tp_block_size * req.tp_block_nr;
    rx->nr_blocks = req.tp_block_nr;
    rx->block_sz = req.tp_block_size;
    return 1;
}

/* 4. Bind to the ifindex on our sending interface */
static int bind_if(rxring_t rx)
{
/*    if ( ifname ) {
        struct ifreq ifr;
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
        if ( ioctl(rx->fd, SIOCGIFINDEX, &ifr) ) {
            perror("ioctl");
            return 0;
        }
        rx->ifindex = ifr.ifr_ifindex;
    }else{
        // interface "any" 
        rx->ifindex = 0;
    }*/

    memset(&sll, 0, sizeof(sll));
    sll.sll_family = PF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = rx->ifindex;
    if ( bind(rx->fd, (struct sockaddr *)&sll, sizeof(sll)) ) {
        perror("bind()");
        return 0;
    }

    return 1;
}

/* 5. finally mmap() the sucker */
static int map_ring(rxring_t rx)
{
    printf("mapping %zu MiB ring buffer\n", rx->map_sz >> 20);
    rx->map = mmap(NULL, rx->map_sz, PROT_READ | PROT_WRITE,
            MAP_SHARED, rx->fd, 0);
    if (rx->map == MAP_FAILED) {
        perror("mmap()");
        return 0;
    }

    return 1;
}

rxring_t rxring_init()
{
    struct _rxring *rx;

    rx = calloc(1, sizeof(*rx));
    if ( NULL == rx )
        goto out;

    if ( !packet_socket(rx) )
        goto out_free;
    if ( !set_v3(rx) )
        goto out_close;
    if ( !rx_ring(rx) )
        goto out_close;
    if ( !bind_if(rx) )
        goto out_close;
    if ( !map_ring(rx) )
        goto out_close;

/*    rx->cb = cb;
    rx->user = user;*/

    /* success */
    goto out;

out_close:
    close(rx->fd);
out_free:
    free(rx);
    rx = NULL;
out:
    return rx;
}

int rxring_fanout_hash(rxring_t rx, uint16_t id)
{
    int val = TPACKET_V3;

    val = PACKET_FANOUT_FLAG_DEFRAG | (PACKET_FANOUT_HASH << 16) | id;

    if (setsockopt(rx->fd, SOL_PACKET, PACKET_FANOUT, &val, sizeof(val))) {
        perror("setsockopt(PACKET_FANOUT)");
        return 0;
    };

    return 1;
}
/*static void hex_dumpf(FILE *f, const uint8_t *tmp, size_t len, size_t llen)
{
    size_t i, j;
    size_t line;

    if ( NULL == f || 0 == len )
        return;
    if ( !llen )
        llen = 0x10;

    for(j = 0; j < len; j += line, tmp += line) {
        if ( j + llen > len ) {
            line = len - j;
        }else{
            line = llen;
        }

        fprintf(f, " | %05zx : ", j);

        for(i = 0; i < line; i++) {
            if ( isprint(tmp[i]) ) {
                fprintf(f, "%c", tmp[i]);
            }else{
                fprintf(f, ".");
            }
        }

        for(; i < llen; i++)
            fprintf(f, " ");

        for(i = 0; i < line; i++)
            fprintf(f, " %02x", tmp[i]);

        fprintf(f, "\n");
    }
    fprintf(f, "\n");
}*/


/* struct ethhdr *ethhdr = (struct ethhdr *)((char *) tpacket_hdr + tpacket_hdr->tp_mac);
                struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
                struct udphdr *udphdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
                char *payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
                char s[INET6_ADDRSTRLEN];
                char s1[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
                inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
                printf("Got message from %s:%d to %s:%d\n", s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
                printf("My port %d their dest port %d\n", ntohs(interface_ep.my_port), ntohs(udphdr->dest) );

                if (udphdr->dest == interface_ep.my_port) {
                    struct in6_memaddr *inAddress =  (struct in6_memaddr *) &iphdr->ip6_dst;
                    int isMyID = 1;
                    if (remoteAddr != NULL && !server) {
                        printf("Their ID\n");
                        printNBytes(inAddress, 16);
                        printf("MY ID\n");
                        printNBytes(remoteAddr, 16);
                        isMyID = (inAddress->cmd == remoteAddr->cmd) && (inAddress->paddr == remoteAddr->paddr);
                    }
                    if (isMyID) {
                        memcpy(receiveBuffer,payload, msgBlockSize);
                        if (remoteAddr != NULL && server) {
                            memcpy(remoteAddr, &iphdr->ip6_dst, IPV6_SIZE);
                        }
                        memcpy(targetIP->sin6_addr.s6_addr, &iphdr->ip6_src, IPV6_SIZE);
                        targetIP->sin6_port = udphdr->source;
                        memset(payload, 0, msgBlockSize);
                        tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                        next_packet(&interface_ep);
                        return msgBlockSize;
                    }*/
/*static struct tpacket3_hdr *do_block (struct tpacket_block_desc *desc, char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int server) {
    uint8_t *ptr;
    struct tpacket3_hdr *hdr;
    unsigned int num_pkts, i;

    ptr = (uint8_t *)desc + desc->hdr.bh1.offset_to_first_pkt;
    num_pkts = desc->hdr.bh1.num_pkts;

    for(i = 0; i < num_pkts; i++) {
        hdr = (struct tpacket3_hdr *)ptr;

        printf("packet %u/%u %u.%u\n",
            i, num_pkts, hdr->tp_sec, hdr->tp_nsec);
        hex_dumpf(stdout, ptr + hdr->tp_mac, hdr->tp_snaplen, 0);
        struct ethhdr *ethhdr = (struct ethhdr *)((char *) hdr + hdr->tp_mac);
        struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
        struct udphdr *udphdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
        char *payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
        if (udphdr->dest == client_my_port) {
                    struct in6_memaddr *inAddress =  (struct in6_memaddr *) &iphdr->ip6_dst;
                    int isMyID = 1;
                    if (remoteAddr != NULL) {
                        isMyID = (inAddress->cmd == remoteAddr->cmd) && (inAddress->paddr == remoteAddr->paddr);
                    }
                    if (isMyID) {
                        memcpy(receiveBuffer,payload, msgBlockSize);
                        if (remoteAddr != NULL && server) {
                            memcpy(remoteAddr, &iphdr->ip6_dst, IPV6_SIZE);
                        }
                        memcpy(targetIP->sin6_addr.s6_addr, &iphdr->ip6_src, IPV6_SIZE);
                        targetIP->sin6_port = udphdr->source;
                        memset(payload, 0, msgBlockSize);
                        return msgBlockSize;
                    }
                }
        ptr += hdr->tp_next_offset;
        __sync_synchronize();
    }
    return NULL;
}*/
#include <arpa/inet.h>
int rxring_mainloop(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int server){
    struct tpacket_block_desc *desc;

    while(!rx_global->cancel) {
        desc = (struct tpacket_block_desc *)
            rx_global->map + rx_global->r_idx * rx_global->block_sz;

        while(!(desc->hdr.bh1.block_status & TP_STATUS_USER))
            poll(&pfd, 1, 0);

        /* walk block */
    uint8_t *ptr;
    struct tpacket3_hdr *hdr;
    unsigned int num_pkts, i;

    ptr = (uint8_t *)desc + desc->hdr.bh1.offset_to_first_pkt;
    num_pkts = desc->hdr.bh1.num_pkts;

    for(i = 0; i < num_pkts; i++) {
        hdr = (struct tpacket3_hdr *)ptr;

/*        printf("packet %u/%u %u.%u\n", i, num_pkts, hdr->tp_sec, hdr->tp_nsec);
        printNBytes(ptr, 100);*/
        //hex_dumpf(stdout, ptr + hdr->tp_mac, hdr->tp_snaplen, 0);
        struct ethhdr *ethhdr = (struct ethhdr *)((char *) hdr + hdr->tp_mac);
        struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
        struct udphdr *udphdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
        char *payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
        /*char s[INET6_ADDRSTRLEN];
        char s1[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
        inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
        printf("Got message from %s:%d to %s:%d\n", s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
        printf("My port %d their dest port %d\n", ntohs(client_my_port), ntohs(udphdr->dest) );*/
        if (udphdr->dest == client_my_port) {
            struct in6_memaddr *inAddress =  (struct in6_memaddr *) &iphdr->ip6_dst;
            int isMyID = 1;
            if (remoteAddr != NULL) {
                isMyID = (inAddress->cmd == remoteAddr->cmd) && (inAddress->paddr == remoteAddr->paddr);
            }
            if (isMyID) {
                memcpy(receiveBuffer,payload, msgBlockSize);
                if (remoteAddr != NULL && server) {
                    memcpy(remoteAddr, &iphdr->ip6_dst, IPV6_SIZE);
                }
                memcpy(targetIP->sin6_addr.s6_addr, &iphdr->ip6_src, IPV6_SIZE);
                targetIP->sin6_port = udphdr->source;
                memset(payload, 0, msgBlockSize);
                desc->hdr.bh1.block_status = TP_STATUS_KERNEL;
                //__sync_synchronize();
                return msgBlockSize;
            }
        }
        ptr += hdr->tp_next_offset;
        //__sync_synchronize();
    }
    desc->hdr.bh1.block_status = TP_STATUS_KERNEL;
    //__sync_synchronize();
    rx_global->r_idx = (rx_global->r_idx + 1) % rx_global->nr_blocks;
    }
  return 0;
}

void rxring_cancel_mainloop(rxring_t rx)
{
    rx->cancel = 1;
}

void rxring_free(rxring_t rx)
{
    if ( rx ) {
        munmap(rx->map, rx->map_sz);
        close(rx->fd);
        free(rx);
    }
}