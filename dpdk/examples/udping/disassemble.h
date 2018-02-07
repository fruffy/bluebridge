#ifndef IPDUMP_DISASSEMBLE_H
#define IPDUMP_DISASSEMBLE_H

#include <rte_mbuf.h>

int
ipdump_disassemble_packet(struct rte_mbuf *m, unsigned portid,
    int enable_filter);

#endif

