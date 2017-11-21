#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#include "pagetable.h"

// Define global variables declared in sim.h
unsigned memsize = 0;
int debug = 0;
char *physmem = NULL;
struct frame *coremap = NULL;
char *tracefile = NULL;


void (*init_fcn)() = NULL;
void (*ref_fcn)(pgtbl_entry_t *) = NULL;
int (*evict_fcn)() = NULL;
