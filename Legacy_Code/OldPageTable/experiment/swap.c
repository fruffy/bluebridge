#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "pagetable.h"
#include "sim.h"
#include "../client_lib.h"
#include "../udpcooked.h"

//---------------------------------------------------------------------
// Bitmap definitions and functions to manage space in swapfile.
// We create a fixed-size swapfile, although it could be made larger
// on demand with a little effort.
//
// The bitmap code is modified from the OS/161 bitmap functions.

#define BITS_PER_WORD 32 // Assumes sizeof(unsigned) = 4 bytes, 32 bits
#define WORD_ALLBITS    (0xffffffff)

#define DIVROUNDUP(a,b) (((a)+(b)-1)/(b))

struct bitmap {
        unsigned nbits;
        unsigned *v;
};

struct bitmap *
bitmap_create(unsigned nbits)
{
        struct bitmap *b; 
        unsigned words;

        words = DIVROUNDUP(nbits, BITS_PER_WORD);
        b = (struct bitmap *)malloc(sizeof(struct bitmap));
        if (b == NULL) {
                return NULL;
        }
        b->v = malloc(words*sizeof(unsigned));
        if (b->v == NULL) {
                free(b);
                return NULL;
        }

        memset(b->v, 0, words*sizeof(unsigned));
        b->nbits = nbits;

        /* Mark any leftover bits at the end in use */
        if (words > nbits / BITS_PER_WORD) {
                unsigned j, ix = words-1;
                unsigned overbits = nbits - ix*BITS_PER_WORD;

                assert(nbits / BITS_PER_WORD == words-1);
                assert(overbits > 0 && overbits < BITS_PER_WORD);
                
                for (j=overbits; j<BITS_PER_WORD; j++) {
                        b->v[ix] |= ((unsigned)1 << j);
                }
        }

        return b;
}

int
bitmap_alloc(struct bitmap *b, unsigned *index)
{
        unsigned ix;
        unsigned maxix = DIVROUNDUP(b->nbits, BITS_PER_WORD);
        unsigned offset;

        for (ix=0; ix<maxix; ix++) {
                if (b->v[ix]!=WORD_ALLBITS) {
                        for (offset = 0; offset < BITS_PER_WORD; offset++) {
                                unsigned mask = ((unsigned)1) << offset;

                                if ((b->v[ix] & mask)==0) {
                                        b->v[ix] |= mask;
                                        *index = (ix*BITS_PER_WORD)+offset;
                                        assert(*index < b->nbits);
                                        return 0;
                                }
                        }
                        assert(0);
                }
        }
        return 1;
}

static
inline
void
bitmap_translate(unsigned bitno, unsigned *ix, unsigned *mask)
{
        unsigned offset;
        *ix = bitno / BITS_PER_WORD;
        offset = bitno % BITS_PER_WORD;
        *mask = ((unsigned)1) << offset;
}

void
bitmap_mark(struct bitmap *b, unsigned index)
{
        unsigned ix;
        unsigned mask;

        assert(index < b->nbits);
        bitmap_translate(index, &ix, &mask);

        assert((b->v[ix] & mask)==0);
        b->v[ix] |= mask;
}

void
bitmap_unmark(struct bitmap *b, unsigned index)
{
        unsigned ix;
        unsigned mask;

        assert(index < b->nbits);
        bitmap_translate(index, &ix, &mask);

        assert((b->v[ix] & mask)!=0);
        b->v[ix] &= ~mask;
}


int
bitmap_isset(struct bitmap *b, unsigned index) 
{
        unsigned ix;
        unsigned mask;

        bitmap_translate(index, &ix, &mask);
        return (b->v[ix] & mask);
}

void
bitmap_destroy(struct bitmap *b)
{
        free(b->v);
        free(b);
}

//---------------------------------------------------------------------
// Swap definitions and functions.

static int sockfd;
struct addrinfo *p = NULL;


int swap_init() {

    struct addrinfo hints, *servinfo;
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "0", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    //Total Time: 1244274 micro seconds
    //Total Time: 71665474 micro seconds

    p = bindSocket(p, servinfo, &sockfd);
    struct sockaddr_in6 *temp = (struct sockaddr_in6 *) p->ai_addr;
    temp->sin6_port = htons(strtol("5000", (char **)NULL, 10));

    genPacketInfo(sockfd);
    openRawSocket();

    return 0;
}

void swap_destroy() {

    // Close and remove swapfile
    close(sockfd);
    closeRawSocket();

    return;
}

// Read data into (simulated) physical memory 'frame' from 'swap_offset'
// in swap file.
// Input:  frame - the physical frame number (not byte offset) in physmem
//         swap_offset - the byte position in the swap file.
// Return: 0 on success, 
//     -errno on error or number of bytes read on partial read
// 
int swap_pagein(unsigned frame, struct i6_ptr remoteMemory) {
    char *frame_ptr;
    // Get pointer to page data in (simulated) physical memory
    frame_ptr = &physmem[frame * SIMPAGESIZE];
/*    printf("Swapin Frame number %d Frame Address %p\n",frame, frame_ptr);
*/    memcpy(frame_ptr, getRemoteMem(sockfd, p, &remoteMemory.r_addr), SIMPAGESIZE);

    return 0;
}

// Write data from (simulated) physical memory 'frame' to 'swap_offset'
// in swap file. Allocates space in swap file for virtual page if needed.
// Input:  frame - the physical frame number (not byte offset in physmem)
//         swap_offset - the byte position in the swap file.
// Return: the swap_offset where the data was written on success,
//         or INVALID_SWAP on failure
// 
int swap_pageout(unsigned frame, struct i6_ptr *swap_offset) {
    char *frame_ptr;


    // Check if swap has already been allocated for this page 
    if (swap_offset->swap_info == INVALID_SWAP) {
        swap_offset->r_addr = allocateRemoteMem(sockfd, p);
        swap_offset->swap_info = VALID_SWAP;
    }
    assert(swap_offset->swap_info != INVALID_SWAP);

    // Get pointer to page data in (simulated) physical memory
    frame_ptr = &physmem[frame * SIMPAGESIZE];
/*    printf("Swapout Frame number %d Frame Address %p\n",frame, frame_ptr);
*/
    writeRemoteMem(sockfd, p, frame_ptr, &swap_offset->r_addr);
    return 0;
}
