/*  Copyright (C) 2011-2015  P.D. Buchan (pdbuchan@yahoo.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef PROJECT_COOK
#define PROJECT_COOK

// Send an IPv6 UDP packet via raw socket at the link layer (ethernet frame).
// Need to have destination MAC address.
// Includes some UDP data.


// Define some constants.
#define ETH_HDRLEN 14  // Ethernet header length
#define IP6_HDRLEN 40  // IPv6 header length
#define UDP_HDRLEN 8  // UDP header length, excludes data
#define IPV6_SIZE 16


/// The number of frames in the ring
//  This number is not set in stone. Nor are block_size, block_nr or frame_size
#define CONF_RING_FRAMES        128
#define CONF_RING_BLOCKS        1
#define FRAMESIZE               8192//(4096 + ETH_HDRLEN + IP6_HDRLEN + UDP_HDRLEN + 2 + 32)
#define BLOCKSIZE               (FRAMESIZE) * (CONF_RING_FRAMES)


#include <stdint.h>         // needed for uint8_t, uint16_t
#include <netinet/ip6.h>    // struct ip6_hdr

#include "config.h"

struct in6_memaddr {
    uint32_t wildcard;
    uint16_t subid;
    uint16_t cmd;
    uint64_t paddr;
};

extern int cooked_send(struct in6_addr *dst_addr, int dst_port, char* data, int datalen);
extern void init_send_socket(struct config *configstruct);
extern void init_send_socket_old(struct config *configstruct);

extern int get_send_socket();
extern int close_send_socket();

extern int cooked_receive(char *receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_addr *remoteAddr);
extern struct sockaddr_in6 *init_rcv_socket(struct config *configstruct);
extern struct sockaddr_in6 *init_rcv_socket_old(struct config *configstruct);
extern int get_rcv_socket();
extern int epoll_rcv();
extern void init_epoll();

extern void close_rcv_socket();
extern int strange_receive();
extern struct sockaddr_in6 *rx_client_init_socket(struct config *configstruct);
extern int rxring_mainloop(char * receiveBuffer, int msgBlockSize, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr, int server);
extern void set_thread_id_sd(int id);
extern void set_thread_id_rx(int id);

#endif
