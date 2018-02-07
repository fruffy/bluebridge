#ifndef IPDUMP_CONFIG_H
#define IPDUMP_CONFIG_H

#define TAHR_IP     "192.168.0.90"
#define DIONE_IP    "192.168.0.118"
#define TAHR_MAC    {0xb8,0xca,0x3a,0x70,0xd7,0x18}  // B8:CA:3A:70:D7:18
#define DIONE_MAC   {0xb8,0xca,0x3a,0x70,0xd8,0xd8}  // B8:CA:3A:70:DB:D8
#define VLAN_ID             10

#define FORMAT_IP_ADDRESS(a,b,c,d) (((a)<<24)+((b)<<16)+((c)<<8)+(d))
#define FORMAT_MAC_ADDRESS(a,b,c,d,e,f) {0x##a,0x##b,0x##c,0x##d,0x##e,0x##f}

#define SRC_PORT     8080
#define DST_PORT     9090

#define UDP_SEND_FREQUENCY 200
#define PING_ID 1034
#endif
