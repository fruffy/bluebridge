#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"

#include <stdbool.h>
#include <string.h>

extern int memsize;

extern int debug;

extern struct frame *coremap;

// where to evict
int fifo_head;


/* Page to evict is chosen using the fifo algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {
  int ans = fifo_head;
  fifo_head = (fifo_head + 1) % memsize;
  return ans;
}

/* This function is called on each access to a page to update any information
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(){
}

/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void fifo_init() {
  fifo_head = 0;
}
