#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netinet/udp.h>      // struct udphdr
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <errno.h>            // errno, perror()
#include <sys/mman.h>         // mmap()
#include <sys/epoll.h>        // epoll_wait(), epoll_event, epoll_rcv()
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

#include <pthread.h>
#include <semaphore.h>
#include <linux/filter.h>
#include <sys/types.h>
#include <sys/timerfd.h>

#include "udpcooked.h"
#include "utils.h"
#include "config.h"


/*          struct sockaddr_in6 {
               sa_family_t     sin6_family;   AF_INET6
               in_port_t       sin6_port;     port number
               uint32_t        sin6_flowinfo; IPv6 flow information
               struct in6_addr sin6_addr;     IPv6 address
               uint32_t        sin6_scope_id; Scope ID (new in 2.4)};
*/

void init_epoll();
void close_epoll();
void *get_free_buffer();
int init_socket();
int set_packet_filter(int sd, char *addr, char* interfaceName, int port);
int set_fanout();

//static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
struct ep_interface {
    struct sockaddr_ll device;
    uint16_t my_port;
    char my_addr[INET6_ADDRSTRLEN];
};
struct rcv_ring {
    struct tpacket_hdr *first_tpacket_hdr;
    int mapped_memory_size;
    struct tpacket_req tpacket_req;
    int tpacket_i;
};
static __thread int epoll_fd = -1;
static struct ep_interface interface_ep;
static __thread struct rcv_ring ring;
static __thread int sd_rcv;
static __thread int thread_id;
const int TIMEOUT = 10;
/* Initialize a listening socket */
struct sockaddr_in6 *init_rcv_socket(struct config *configstruct) {
    struct sockaddr_in6 *temp = malloc(sizeof(struct sockaddr_in6));
    inet_ntop(AF_INET6, &configstruct->src_addr, interface_ep.my_addr, INET6_ADDRSTRLEN);
    if (!interface_ep.my_port) {
        interface_ep.my_port = configstruct->src_port;
        if ((interface_ep.device.sll_ifindex = if_nametoindex (configstruct->interface)) == 0) {
            perror ("if_nametoindex() failed to obtain interface index ");
            exit (EXIT_FAILURE);
        }

        interface_ep.device.sll_family = AF_PACKET;
        interface_ep.device.sll_protocol = htons (ETH_P_ALL);
    }
    interface_ep.device.sll_ifindex = if_nametoindex (configstruct->interface);
    printf("interface name: %s\n",configstruct->interface);
    printf("ip:%d\n",interface_ep.device.sll_ifindex);
    init_socket();
    set_packet_filter(sd_rcv, interface_ep.my_addr, configstruct->interface,htons((ntohs(interface_ep.my_port) + thread_id)));
    init_epoll();
    if (-1 == bind(sd_rcv, (struct sockaddr *)&interface_ep.device, sizeof(interface_ep.device))) {
        perror("Could not bind socket.");
        exit(1);
    }
    return temp;
}

int set_fanout() {
    int fanout_group_id = getpid() & 0xffff;
    if (fanout_group_id) {
            // PACKET_FANOUT_LB - round robin
            // PACKET_FANOUT_CPU - send packets to CPU where packet arrived
            int fanout_type = PACKET_FANOUT_CPU; 

            int fanout_arg = (fanout_group_id | (fanout_type << 16));

            int setsockopt_fanout = setsockopt(sd_rcv, SOL_PACKET, PACKET_FANOUT, &fanout_arg, sizeof(fanout_arg));

            if (setsockopt_fanout < 0) {
                printf("Can't configure fanout\n");
               return EXIT_FAILURE;
            }
    }
    return EXIT_SUCCESS;
}

void set_thread_id_rx(int id) {
    thread_id = id;
}

int set_packet_filter(int sd, char *addr, char *interfaceName, int port) {
    struct sock_fprog filter;
    int i, lineCount = 0;
    char tcpdump_command[512];
    FILE* tcpdump_output;
    sprintf(tcpdump_command, "tcpdump -i %s dst port %d and ip6 net %s/48 -ddd",interfaceName, ntohs(port), addr);
    printf("Active Filter: %s\n",tcpdump_command );
    if ( (tcpdump_output = popen(tcpdump_command, "r")) == NULL ) {
        RETURN_ERROR(-1, "Cannot compile filter using tcpdump.");
    }
    if ( fscanf(tcpdump_output, "%d\n", &lineCount) < 1 ) {
        RETURN_ERROR(-1, "cannot read lineCount.");
    }
    filter.filter = calloc(sizeof(struct sock_filter)*lineCount,1);
    filter.len = lineCount;
    for ( i = 0; i < lineCount; i++ ) {
        if (fscanf(tcpdump_output, "%u %u %u %u\n", (unsigned int *)&(filter.filter[i].code),(unsigned int *) &(filter.filter[i].jt),(unsigned int *) &(filter.filter[i].jf), &(filter.filter[i].k)) < 4 ) {
            free(filter.filter);
            RETURN_ERROR(-1, "fscanf: error in reading");
    }
    setsockopt(sd, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter));
    }
    pclose(tcpdump_output);
    return EXIT_SUCCESS;
}

/* Initialize a listening socket */
struct sockaddr_in6 *init_rcv_socket_old(struct config *configstruct) {

    struct addrinfo hints, *servinfo, *p = NULL;
    int rv;
    //Socket operator variables
    const int on=1, off=0;
    // hints = specifies criteria for selecting the socket address
    // structures
    interface_ep.my_port = configstruct->src_port;
    memset (&interface_ep.device, 0, sizeof (interface_ep.device));
    if ((interface_ep.device.sll_ifindex = if_nametoindex (configstruct->interface)) == 0) {
        perror ("if_nametoindex() failed to obtain interface index ");
        exit (EXIT_FAILURE);
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    char str_port[5];
    sprintf(str_port, "%d", configstruct->src_port);
    if ((rv = getaddrinfo(NULL, str_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sd_rcv = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
                == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sd_rcv, SOL_SOCKET, SO_REUSEADDR, &off, sizeof(int))
                == -1) {
            perror("setsockopt");
            exit(1);
        }
        setsockopt(sd_rcv, IPPROTO_IPV6, IP_PKTINFO, &on, sizeof(int));
        setsockopt(sd_rcv, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(int));
        setsockopt(sd_rcv, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(int));
        ((struct sockaddr_in6 *)p->ai_addr)->sin6_port = interface_ep.my_port;
        if (bind(sd_rcv, p->ai_addr, p->ai_addrlen) == -1) {
            close(sd_rcv);
            perror("server: bind");
            continue;
        }
        break;
    }
    struct sockaddr_in6 *temp = malloc(sizeof(struct sockaddr_in6));
    memcpy(temp, p->ai_addr, sizeof(struct sockaddr_in6));

    return temp;
}


int get_rcv_socket() {
    return sd_rcv;
}

void close_rcv_socket() {
    close(sd_rcv);
    sd_rcv = -1;
    close_epoll();
}

int setup_packet_mmap() {

    struct tpacket_req tpacket_req = {
        .tp_block_size = BLOCKSIZE,
        .tp_block_nr = CONF_RING_BLOCKS,
        .tp_frame_size = FRAMESIZE,
        .tp_frame_nr = CONF_RING_BLOCKS * (BLOCKSIZE/ FRAMESIZE)
    };

    int size = tpacket_req.tp_block_size *tpacket_req.tp_block_nr;

    void *mapped_memory = NULL;

    if (setsockopt(sd_rcv, SOL_PACKET, PACKET_RX_RING, &tpacket_req, sizeof tpacket_req)) {
        return -1;
    }

    mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, sd_rcv, 0);

    if (MAP_FAILED == mapped_memory) {
        return -1;
    }

    ring.first_tpacket_hdr = mapped_memory;
    ring.mapped_memory_size = size;
    ring.tpacket_req = tpacket_req;

    return 0;
}


int init_socket() {
    sd_rcv = socket(AF_PACKET, SOCK_RAW|SOCK_NONBLOCK, htons(ETH_P_ALL));

    //const int on = 1;
    //setsockopt(sd_rcv, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int));
    //setsockopt(sd_rcv, SOL_PACKET, PACKET_QDISC_BYPASS, &on, sizeof(on));
    //setsockopt(sd_rcv, SOL_PACKET, SO_BUSY_POLL, &on, sizeof(on));
    if (-1 == sd_rcv) {
        perror("Could not set socket");
        return EXIT_FAILURE;
    }

    return 0;
}

void init_epoll() {

    if (setup_packet_mmap()) {
        close(sd_rcv);
        exit(1);
    }
    struct epoll_event event = {
        .events = EPOLLIN,
        .data = {.fd = sd_rcv }
    };

    epoll_fd = epoll_create1(0);

    if (-1 == epoll_fd) {
        perror("epoll_create failed.");
       exit(1);
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd_rcv, &event)) {
        perror("epoll_ctl failed");
        close(epoll_fd);
       exit(1);
    }
}

void close_epoll() {
    close(epoll_fd);
    munmap(ring.first_tpacket_hdr, ring.mapped_memory_size);
}

struct tpacket_hdr *get_packet(struct rcv_ring *ring_p) {
    return (void *)((char *)ring_p->first_tpacket_hdr + ring_p->tpacket_i * ring_p->tpacket_req.tp_frame_size);
}

void next_packet(struct rcv_ring *ring_p) {
   ring_p->tpacket_i = (ring_p->tpacket_i + 1) % ring_p->tpacket_req.tp_frame_nr;
}

int epoll_rcv(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int server) {
    while (1) {
        struct epoll_event events[1024];

        int num_events = epoll_wait(epoll_fd, events, sizeof events / sizeof *events, TIMEOUT);

        if (num_events == 0 && !server) {
            printf("TIMEOUT!\n");
            return -1;
        }

        for (int i = 0; i < num_events; i++)  {
            struct epoll_event *event = &events[i];
            if (event->events & EPOLLIN) {
                struct tpacket_hdr *tpacket_hdr = get_packet(&ring);
                if ( tpacket_hdr->tp_status == TP_STATUS_KERNEL) {
                    if (!server)
                        next_packet(&ring);
                    continue;
                }
                if (tpacket_hdr->tp_status & TP_STATUS_COPY) {
                    next_packet(&ring);
                    continue;
                }
                if (tpacket_hdr->tp_status & TP_STATUS_LOSING) {
                    next_packet(&ring);
                    continue;
                }
                struct ethhdr *ethhdr = (struct ethhdr *)((char *) tpacket_hdr + tpacket_hdr->tp_mac);
                struct ip6_hdr *iphdr = (struct ip6_hdr *)((char *)ethhdr + ETH_HDRLEN);
                struct udphdr *udphdr = (struct udphdr *)((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN);
                char *payload = ((char *)ethhdr + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN);
                /*
                char s[INET6_ADDRSTRLEN];
                char s1[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &iphdr->ip6_src, s, sizeof s);
                inet_ntop(AF_INET6, &iphdr->ip6_dst, s1, sizeof s1);
                printf("Thread %d Got message from %s:%d to %s:%d\n", thread_id, s,ntohs(udphdr->source), s1, ntohs(udphdr->dest) );
                printf("Thread %d My port %d their dest port %d\n",thread_id, ntohs(interface_ep.my_port), ntohs(udphdr->dest) );
                */
                struct in6_memaddr *inAddress =  (struct in6_memaddr *) &iphdr->ip6_dst;
                int isMyID = 1;
                if (remoteAddr != NULL && !server) {
                    //printf("Thread %d Their ID\n", thread_id);
                    //printNBytes(inAddress, 16);
                    //printf("Thread %d MY ID\n", thread_id);
                    //printNBytes(remoteAddr, 16)
                    isMyID = (inAddress->cmd == remoteAddr->cmd) && (inAddress->paddr == remoteAddr->paddr);
                }
                if (isMyID) {
                    memcpy(receiveBuffer, payload, msgBlockSize);
                    if (remoteAddr != NULL && server) {
                        memcpy(remoteAddr, &iphdr->ip6_dst, IPV6_SIZE);
                    }
                    //printf("rec: %s\n",receiveBuffer);
                    memcpy(targetIP->sin6_addr.s6_addr, &iphdr->ip6_src, IPV6_SIZE);
                    targetIP->sin6_port = udphdr->source;
                    tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                    next_packet(&ring);
                    return msgBlockSize;
                }
                tpacket_hdr->tp_status = TP_STATUS_KERNEL;
                next_packet(&ring);
           }
        }
    }
    return EXIT_SUCCESS;
}

int cooked_receive(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr *ipv6Pointer){
    struct sockaddr_in6 from;
    struct iovec iovec[1];
    struct msghdr msg;
    char msg_control[1024];
    char udp_packet[msgBlockSize];
    int numbytes = 0;
    //char s[INET6_ADDRSTRLEN];
    iovec[0].iov_base = udp_packet;
    iovec[0].iov_len = sizeof(udp_packet);
    msg.msg_name = &from;
    msg.msg_namelen = sizeof(from);
    msg.msg_iov = iovec;
    msg.msg_iovlen = sizeof(iovec) / sizeof(*iovec);
    msg.msg_control = msg_control;
    msg.msg_controllen = sizeof(msg_control);
    msg.msg_flags = 0;

    print_debug("Waiting for response...");
    memset(receiveBuffer, 0, msgBlockSize);
    numbytes = recvmsg(sd_rcv, &msg, 0);
    struct in6_pktinfo * in6_pktinfo;
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != 0; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
            in6_pktinfo = (struct in6_pktinfo*)CMSG_DATA(cmsg);
            //inet_ntop(targetIP->sin6_family, &in6_pktinfo->ipi6_addr, s, sizeof s);
            //print_debug("Received packet was sent to this IP %s",s);
            if (ipv6Pointer != NULL)
                memcpy(ipv6Pointer->s6_addr,&in6_pktinfo->ipi6_addr,IPV6_SIZE);
            memcpy(receiveBuffer,iovec[0].iov_base,iovec[0].iov_len);
            memcpy(targetIP, (struct sockaddr *) &from, sizeof(from));
        }
    }

    //inet_ntop(targetIP->sin6_family, targetIP, s, sizeof s);
    //print_debug("Got message from %s:%d", s, ntohs(targetIP->sin6_port));
    //printNBytes(receiveBuffer, 50);
    return numbytes;
}
