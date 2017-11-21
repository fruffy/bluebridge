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

// bitmap recording referenced frames
bool* clk_refed;
// where to start searching for unreferenced frame
int clk_hand;


/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
  // search for the first unreferenced frame
  // starting from clk_hand
  int i;
  for (i = clk_hand; i < memsize; i = (i + 1) % memsize) {
    // flip reference bit
    if (clk_refed[i]) {
      clk_refed[i] = 0;
    } else { // found
      clk_refed[i] = 1;
      // update clk_hand for next run
      clk_hand = i; 
      return i;
    }
  }

  // this shouldn't happen
  assert(0);
}


/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {
  int frame = p->frame >> PAGE_SHIFT;
  clk_refed[frame] = 1;
}


/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
  clk_hand = 0;
  clk_refed = malloc(sizeof(bool) * memsize);
  memset(clk_refed, 0, sizeof(bool) * memsize);
}
