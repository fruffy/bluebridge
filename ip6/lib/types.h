#ifndef PROJECT_TYPES
#define PROJECT_TYPES
#include <stdint.h> // uint16_t, uint32_t, uint64_t
/**
 * This files contains definitions and types which are used everywhere in the code.
 * It should generally be included when working with the BlueBridge library. For now it is 
 * exported by server_lib and client_lib by default.
 */


// Get used to dealing with this structure a lot... 
/*          struct sockaddr_in6 {
               sa_family_t     sin6_family;   AF_INET6
               in_port_t       sin6_port;     port number
               uint32_t        sin6_flowinfo; IPv6 flow information
               struct in6_addr sin6_addr;     IPv6 address
               uint32_t        sin6_scope_id; Scope ID (new in 2.4)};
*/

/**
 * @brief ip6_memaddr
 * @details Defines the structure of the IPv6 address, which is the
 * basis of all the system's communication. It is generally
 * used as a key which also functions as destination IP address.
 */
typedef struct ip6_memaddr {
    uint16_t subid; // Specifies the IPv6 Subnet
    uint16_t cmd;   // The cmd that is to be executed on the server
    uint32_t args;  // An argument wildcard, can be defined arbitrarily.
    uint64_t paddr; // The pointer the client is operating on
}ip6_memaddr;

/**
 * @brief ip6_memaddr_block
 * @details A block version of ip6_memaddr. The ip6_memaddrs in this struct
 * points to a larger allocated block on the server machine. The length specifies
 * the number of BLOCK_SIZE pointers which can be read from this key. * 
 */
typedef struct ip6_memaddr_block {
  struct ip6_memaddr memaddr; // Starting address
  uint64_t length;            // BLOCK_SIZE * length defines the size of the pointer range
}ip6_memaddr_block;

/**
 * @brief pkt_rqst
 * @details A useful little type which defines the packet request that is usually sent
 * out in BlueBridge. pkt_rqst contains all the necessary information to generate a BlueBridge
 * client request. 
 * 
 */
typedef struct pkt_rqst {
    ip6_memaddr dst_addr; // The destination of the packet
    int dst_port;         // Port we want to send to
    char *data;           // Pointer to the data we want to send
    int datalen;          // Size of the data we want to send
}pkt_rqst;


// Define some constants.
#define ETH_HDRLEN 14   // Ethernet header length
#define IP6_HDRLEN 40   // IPv6 header length
#define UDP_HDRLEN 8    // UDP header length, excludes data
#define IPV6_SIZE 16    // Length of an IPV6 Address
#define BLOCK_SIZE 4096 // Our current default transport size
#define POINTER_SIZE sizeof(void*)


// A list of blueBridge commands
// These are inserted into ip6_memaddr.cmd
#define ALLOC_CMD       01
#define WRITE_CMD       02
#define READ_CMD        03
#define FREE_CMD        04
#define ALLOC_BULK_CMD  05
#define WRITE_BULK_CMD  06
#define READ_BULK_CMD   07
#define CMD_SIZE        02

// Offset of data from start of frame
#define PKT_OFFSET      (TPACKET_ALIGN(sizeof(struct tpacket_hdr)) + \
                         TPACKET_ALIGN(sizeof(struct sockaddr_ll)))
// Macro to acquire the size of an array
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#endif
