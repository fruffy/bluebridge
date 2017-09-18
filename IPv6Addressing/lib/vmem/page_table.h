#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <errno.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
struct page_table;
typedef void (*page_fault_handler_t) ( struct page_table *pt, int page );
struct page_table {
    int fd;
    char *virtmem;
    int npages;
    char *physmem;
    int nframes;
    int *page_mapping;
    int *page_bits;
    page_fault_handler_t handler;
};


/* Create a new page table, along with a corresponding virtual memory
that is "npages" big and a physical memory that is "nframes" big
 When a page fault occurs, the routine pointed to by "handler" will be called. */

struct page_table * page_table_create( int npages, int nframes, page_fault_handler_t handler );

/* Delete a page table and the corresponding virtual and physical memories. */

void page_table_delete( struct page_table *pt );

void page_table_flush(struct page_table *pt);

/*
Set the frame number and access bits associated with a page.
The bits may be any of PROT_READ, PROT_WRITE, or PROT_EXEC logical-ored together.
*/

void page_table_set_entry( struct page_table *pt, int page, int frame, int bits );

/*
Get the frame number and access bits associated with a page.
"frame" and "bits" must be pointers to integers which will be filled with the current values.
The bits may be any of PROT_READ, PROT_WRITE, or PROT_EXEC logical-ored together.
*/

void page_table_get_entry( struct page_table *pt, int page, int *frame, int *bits );

/* Return a pointer to the start of the virtual memory associated with a page table. */

char * page_table_get_virtmem( struct page_table *pt );

/* Return a pointer to the start of the physical memory associated with a page table. */

char * page_table_get_physmem( struct page_table *pt );

/* Return the total number of frames in the physical memory. */

int page_table_get_nframes( struct page_table *pt );

/* Return the total number of pages in the virtual memory. */

int page_table_get_npages( struct page_table *pt );

/* Print out the page table entry for a single page. */

void page_table_print_entry( struct page_table *pt, int page );

/* Print out the state of every page in a page table. */

void page_table_print( struct page_table *pt );

void set_vmem_config(char *filename);

struct page_table *init_virtual_memory(int npages, int nframes, const char* algo);

void print_page_faults();

void clean_page_table(struct page_table *pt);

#endif
