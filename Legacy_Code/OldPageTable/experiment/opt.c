#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"

// will give me
// extern char* tracefile
// MAXLINE
#include "sim.h"

// will give me conflicting types int vs unsigned
// extern int memsize;

extern int debug;

extern struct frame *coremap;

typedef struct llnode_t {
  // vaddr, used to identify same page
  addr_t vaddr;
  // pointer to next llnode
  struct llnode_t* next;
} llnode;

// head of trace list
llnode* opt_head;
// current trace working on
llnode* opt_curr;
// distance for m options
int* opt_dist;

int debug_count = 0;

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
  addr_t target_vaddr;
  uint32_t i;
  for (i = 0; i < memsize; i++) {
    // looking for this vaddr in the following trace
    target_vaddr = coremap[i].vaddr;

    // how soon will a frame i be visited
    int distance = 0;

    // start of traversal, current trace node
    llnode* p = opt_curr->next;
    while (p && p->vaddr != target_vaddr) {
      distance++;
      p = p->next;
    }

    if (p) { // found target
      opt_dist[i] = distance;
    } else { // frame never show up again, best candidate
      return i;
    }
  }

  // every frame has some future occurrence, find longest distance
  int longest_dist = -1;
  int longest_frame = -1;
  for (i = 0; i < memsize; i++) {
    if (opt_dist[i] > longest_dist) {
      longest_frame = i;
      longest_dist = opt_dist[i];
    }
  }
  return longest_frame;

}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref() {
  debug_count++;
  // simply move the current node to next node
  opt_curr = opt_curr->next;
  // circular, deal with verify_page_versions
  if (!opt_curr) {
    opt_curr = opt_head;
  }
  return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
  FILE* tfp = fopen(tracefile, "r");
  char buf[MAXLINE];
  addr_t vaddr;
  char type;

  opt_head = NULL;
  opt_curr = NULL;

  while (fgets(buf, MAXLINE, tfp) != NULL) {
    if (buf[0] != '=') {
      sscanf(buf, "%c %lx", &type, &vaddr);

      // create new node, store vaddr
      llnode* new_node = malloc(sizeof(llnode));
      new_node->vaddr = (vaddr >> PAGE_SHIFT) << PAGE_SHIFT;
      new_node->next = NULL;

      // properly add to list
      if (opt_head == NULL) {
        opt_head = new_node;
        opt_curr = new_node;
      } else {
        opt_curr->next = new_node;
        opt_curr = new_node;
      }

    }
  }

  opt_curr = opt_head;

  opt_dist = malloc(sizeof(int) * memsize);

}
