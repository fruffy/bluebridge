#ifndef PROJECT_COOK
#define PROJECT_COOK
#include "../config.h"
#include "../types.h"

// tp_block_size must be a multiple of PAGE_SIZE (1)
// tp_frame_size must be greater than TPACKET_HDRLEN (obvious)
// tp_frame_size must be a multiple of TPACKET_ALIGNMENT
// tp_frame_nr   must be exactly frames_per_block*tp_block_nr

//  This number is not set in stone. Nor are block_size, block_nr or frame_size
#define C_RING_FRAMES        1024 //16384 // The number of frames per block
#define RX_RING_BLOCKS       16   // NUM BLOCKS for the RX RING
#define TX_RING_BLOCKS       1    // NUM BLOCKS for the TX RING
#define C_FRAMESIZE          8192 //(4096 + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN + 2 + 32)
#define C_BLOCKSIZE          (C_FRAMESIZE) * (C_RING_FRAMES)

struct rx_ring {
    struct tpacket_hdr *first_tpacket_hdr;
    int mapped_memory_size;
    struct tpacket_req tpacket_req;
    uint64_t tpacket_i;
};

extern int cooked_send(pkt_rqst pkt);
extern int cooked_batched_send(pkt_rqst *pkts, int num_pkts, uint32_t *sub_ids);

extern int simple_epoll_rcv(char *rcv_buf, struct sockaddr_in6 *target_ip, ip6_memaddr *remote_addr);
extern int epoll_server_rcv(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, ip6_memaddr *remoteAddr);
void write_packets(int num_packets);
extern void init_tx_socket(struct config *configstruct);
extern void init_simple_rx_socket(struct config *cfg);
extern void init_rx_socket_server(struct config *configstruct);
extern void enter_raw_server_loop(uint16_t server_port);

extern int get_tx_socket();
extern int close_tx_socket();
extern void close_rx_socket_client();
extern void close_rx_socket_server();

extern void set_thread_id_tx(int id);
extern void set_thread_id_rx_client(int id);
extern void set_thread_id_rx_server(int id);

extern int setup_rx_socket();
extern void next_packet(struct rx_ring *ring_p);
extern struct tpacket_hdr *get_packet(struct rx_ring *ring_p);
extern void close_epoll(int epoll_fd, struct rx_ring ring_rx);
extern void close_poll(struct rx_ring ring_rx);

#endif
