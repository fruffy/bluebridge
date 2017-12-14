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

// use a linked list to track the lru queue
typedef struct llnode_t {
  // frame number
  int frame;
  // pointer to next llnode
  struct llnode_t* next;
} llnode;

// main lru queue, record the sequence of insertion
// head - evict, tail - insert
llnode* lru_head;
llnode* lru_tail;
// bitmap recording referenced frames
bool* lru_refed;


/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
  // ensure there is a frame to evict
  assert(lru_head != NULL);

  // recover frame number from lru_head of list
  int frame = lru_head->frame;

  // if only one element in the list
  if (lru_head == lru_tail) {
    lru_tail = NULL;
  }

  // extra error checking
  assert(lru_refed[frame] == 1);
  
  // mark frame as unreferenced
  lru_refed[frame] = 0;

  // update the lru_head of list
  llnode* new_lru_head = lru_head->next;
  free(lru_head);
  lru_head = new_lru_head;

  return frame;
}


/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

  int frame = p->frame >> PAGE_SHIFT;

  // if not recently referenced
  if (lru_refed[frame] == 0) {

    // mark frame as referenced
    lru_refed[frame] = 1;

    // create a new llnode to record the frame
    llnode* new_node = (llnode*)malloc(sizeof(llnode));
    new_node->frame = frame;
    new_node->next = NULL;

    // update the lru_tail of list
    if (lru_tail == NULL) { // list empty
      lru_tail = new_node;
      lru_head = lru_tail;
    } else { // list not empty
      lru_tail->next = new_node;
      lru_tail = new_node;
    }

  } else {

    // create a new llnode to record the frame
    llnode* new_node = (llnode*)malloc(sizeof(llnode));
    new_node->frame = frame;
    new_node->next = NULL;
    
    // append new_node to lru_tail of list
    lru_tail->next = new_node;
    lru_tail = new_node;
    
    // remove the last reference from list
    llnode* pn = lru_head;
    llnode* prev = NULL;
    while (pn->frame != frame) {
      prev = pn;
      pn = pn->next;
    }

    if (prev != NULL) {
      prev->next = pn->next;
      free(pn);
    } else { // about to delete lru_head
      lru_head = pn->next;
      free(pn);
      if (lru_head == NULL) { // list become empty
        lru_tail = NULL;
      }
    }

  }

  return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
  lru_head = NULL;
  lru_tail = NULL;
  lru_refed = malloc(sizeof(bool) * memsize);
  memset(lru_refed, 0, sizeof(bool) * memsize);
}
