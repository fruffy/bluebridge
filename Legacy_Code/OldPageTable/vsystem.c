#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "./lib/experiment/pagetable.h"
#include "./lib/experiment/sim.h"
#include "./lib/client_lib.h"

/* The algs array gives us a mapping between the name of an eviction
 * algorithm as given in a command line argument, and the function to
 * call to select the victim page.
 */
struct functions algs[] = {
  {"rand", rand_init, rand_ref, rand_evict}, 
  {"lru", lru_init, lru_ref, lru_evict},
  {"fifo", fifo_init, fifo_ref, fifo_evict},
  {"clock",clock_init, clock_ref, clock_evict},
  {"opt", opt_init, opt_ref, opt_evict}
};
int num_algs = 5;

/* An actual memory access based on the vaddr from the trace file.
 *
 * The find_physpage() function is called to translate the virtual address
 * to a (simulated) physical address -- that is, a pointer to the right
 * location in physmem array. The find_physpage() function is responsible for
 * everything to do with memory management - including translation using the
 * pagetable, allocating a frame of (simulated) physical memory (if needed),
 * evicting an existing page from the frame (if needed) and reading the page
 * in from swap (if needed).
 *
 * We then check that the memory has the expected content (just a copy of the
 * virtual address) and, in case of a write reference, increment the version
 * counter. 
 */
void access_mem(char type, addr_t vaddr) {
  char *memptr = find_physpage(vaddr, type);
  int *versionptr = (int *)memptr;
  addr_t *checkaddr = (addr_t *)(memptr + sizeof(int));

  if (*checkaddr != vaddr) {
    //fprintf(stderr,"Error, simulated page returned by pagetable lookup doese not have expected value.\n");
  }
  
  if (type == 'S' || type == 'M') {
    // write access to page, increment version number
    (*versionptr)++;
  }

}

uint64_t total_latency = 0;

void replay_trace(FILE *infp) {
  char buf[MAXLINE];
  addr_t vaddr = 0;
  char type;
  while(fgets(buf, MAXLINE, infp) != NULL) {
    if(buf[0] != '=') {
      sscanf(buf, "%c %lx", &type, &vaddr);
      if(debug)  {
        printf("%c %lx\n", type, vaddr);
      }
      uint64_t rStart = getns();
      access_mem(type, vaddr);
      uint64_t latency = getns() - rStart;
      total_latency +=latency;
      /*if (latency/1000 > 0)
        printf("Memory Access: %lu micro seconds\n", latency/1000);*/
            } else {
      continue;
    }
  }
}

int main(int argc, char *argv[]) {
  int opt;
  unsigned swapsize;
  FILE *tfp = stdin;
  char *replacement_alg = NULL;
  char *usage = "USAGE: sim -f tracefile -m memorysize -s swapsize -a algorithm\n";

  while ((opt = getopt(argc, argv, "f:m:a:s:")) != -1) {
    switch (opt) {
    case 'f':
      tracefile = optarg;
      break;
    case 'm':
      memsize = (unsigned)strtoul(optarg, NULL, 10);
      break;
    case 'a':
      replacement_alg = optarg;
      break;
    case 's':
      swapsize = (unsigned)strtoul(optarg, NULL, 10);
      break;
    default:
      fprintf(stderr, "%s", usage);
      exit(1);
    }
  }
  if(tracefile != NULL) {
    if((tfp = fopen(tracefile, "r")) == NULL) {
      perror("Error opening tracefile:");
      exit(1);
    }
  }

  // Initialize main data structures for simulation.
  // This happens before calling the replacement algorithm init function
  // so that the init_fcn can refer to the coremap if needed.

  coremap = malloc(memsize * sizeof(struct frame));
  physmem = malloc(memsize * SIMPAGESIZE);
  printf("%d\n", memsize * SIMPAGESIZE);
  swap_init(swapsize);
  init_pagetable();


  // Initialize replacement algorithm functions.
  if(replacement_alg == NULL) {
    fprintf(stderr, "%s", usage);
    exit(1);
  } else {
    int i;
    for (i = 0; i < num_algs; i++) {
      if(strcmp(algs[i].name, replacement_alg) == 0) {
        init_fcn = algs[i].init;
        ref_fcn = algs[i].ref;
        evict_fcn = algs[i].evict;
        break;
      }
    }
    if(evict_fcn == NULL) {
      fprintf(stderr, "Error: invalid replacement algorithm - %s\n", 
          replacement_alg);
      exit(1);
    }
  }
  // Call replacement algorithm's init_fcn before replaying trace.
  init_fcn();

  replay_trace(tfp);
/*  print_pagedirectory();
*/
  // Cleanup - removes temporary swapfile.
  swap_destroy();

  printf("\n");
  printf("Hit count: %d\n", hit_count);
  printf("Miss count: %d\n", miss_count);
  printf("Clean evictions: %d\n",evict_clean_count);
  printf("Dirty evictions: %d\n",evict_dirty_count); 
  printf("Total references : %d\n", ref_count);
  printf("Hit rate: %.4f\n", (double)hit_count/ref_count * 100);
  printf("Miss rate: %.4f\n", (double)miss_count/ref_count *100);
  printf("Total Latency %lu ms\n", total_latency/ 1000);
  return(0);
}
