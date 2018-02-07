/*-
 * Copyright (c) 2010 David Malone <dwmalone@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include "rss.h"

#ifdef EXTENDED_TOEPLITZ
#define TOEPLITZ_LEN            52
#else
#define TOEPLITZ_LEN            40
#endif
#define BIT_SET(data, idx)      (data & (1 << idx))
#define RSS_BIT_MASK            0x0000007F
#define RSS_FIELDS_LEN          12

static uint8_t rss_intel_key[TOEPLITZ_LEN] = {
    0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
    0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
    0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
    0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
    0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
#ifdef EXTENDED_TOEPLITZ
    0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
    0xD0, 0xCA, 0x2B, 0xCB,
#endif
};

static uint32_t
toeplitz_hash(int keylen, const uint8_t *key,
              int datalen, const uint8_t *data)
{
    uint32_t hash = 0, v;
    int i, b;

    /* XXXRW: Perhaps an assertion about key length vs. data length? */

    v = (key[0]<<24) + (key[1]<<16) + (key[2]<<8) + key[3];
    for (i = 0; i < datalen; i++) {
        for (b = 0; b < 8; b++) {
            if (data[i] & (1<<(7-b)))
                hash ^= v;
            v <<= 1;
            if ((i + 4) < keylen &&
                    (key[i+4] & (1<<(7-b))))
                v |= 1;
        }
    }
    return hash;
}

int
GetRSSCPUCore(uint32_t sip, uint32_t dip,
              uint16_t sp, uint16_t dp, int num_queues)
{
    uint8_t hdr_bytes[RSS_FIELDS_LEN];
    uint32_t res;

    *((uint32_t *) &hdr_bytes[0]) = sip;
    *((uint32_t *) &hdr_bytes[4]) = dip;
    *((uint16_t *) &hdr_bytes[8]) = sp;
    *((uint16_t *) &hdr_bytes[10]) = dp;

    res = toeplitz_hash(TOEPLITZ_LEN, rss_intel_key,
                        RSS_FIELDS_LEN, hdr_bytes);

    return ((int) (res & RSS_BIT_MASK)) % num_queues;
}

int
GetSourcePortForCore(char *dst_ip, char *src_ip,
                     uint16_t dst_p, uint16_t src_start,
                     int dest_queues, int des_core)
{
    uint32_t sip, dip;
    uint16_t sport, dp;

    if (!inet_pton(AF_INET, src_ip, &sip)) {
        perror("local ip failure");
        exit(EXIT_FAILURE);
    }

    if (!inet_pton(AF_INET, dst_ip, &dip)) {
        perror("remote ip failure");
        exit(EXIT_FAILURE);
    }

    sport = src_start;
    dp = htons(dst_p);

    while (sport < 0xFFFF) {
        int rcore = GetRSSCPUCore(dip, sip, dp, htons(sport), dest_queues);
        if (rcore == des_core)
            break;
        sport++;
    }

    return sport;
}
