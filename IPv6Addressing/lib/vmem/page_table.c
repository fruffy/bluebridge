#define _GNU_SOURCE

#include "page_table.h"
#include "rmem.h"
#include "rrmem.h"
#include "disk.h"
#include "mem.h"


int* frameState; // keeps track of how long a page has been in a frame
int* framePage; // keeps track of which page is in a frame
char* physmem;
int pageFaults = 0;
int pageReads = 0;
int pageWrites = 0;
const char* pagingSystem;
struct page_table *the_page_table = 0;


static void internal_fault_handler( int signum, siginfo_t *info, void *context ) {

    (void) signum;
    char *addr = info->si_addr;
    (void) context;
    struct page_table *pt = the_page_table;

    if(pt) {
        int page = (addr-pt->virtmem) / PAGE_SIZE;

        if(page>=0 && page<pt->npages) {
            pt->handler(pt,page);
            return;
        }
    }

    fprintf(stderr,"segmentation fault at address %p\n",addr);
    abort();
}

// Remote Raided memory handler
struct rrmem* rrmem;
void page_fault_handler_rrmem( struct page_table *pt, int page ) {
    int i;
    int frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = 0; i < pt->nframes; i++) { // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }

    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = 0; i < pt->nframes; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                page_table_set_entry(pt, page, i, PROT_READ);
                rrmem_read(rrmem, page, &physmem[i*PAGE_SIZE]);
                pageReads++;
                break;
            }
        }
        // first in first out page replacement
        if (emptyFrame == 0) {
            int value = 0;
            for(i = 0; i < pt->nframes; i++) { // get oldest page
                if(frameState[i] > value) {
                    value = frameState[i];
                    frame = i;
                }
            }
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                rrmem_write(rrmem, tempPage, &physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            rrmem_read(rrmem, page, &physmem[frame*PAGE_SIZE]);
            pageReads++;
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            frameState[frame] = 1;
            framePage[frame] = page;
        }
    } else { // if page is already in table--need to set write bit
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
        frameState[frame] = 1;
        framePage[frame] = page;
    }
}

// TODO: Add compile IFDEFS to save space
struct rmem* rmem;
void page_fault_handler_rmem( struct page_table *pt, int page ) {
    int i;
    int frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = 0; i < pt->nframes; i++) { // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }

    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = 0; i < pt->nframes; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                page_table_set_entry(pt, page, i, PROT_READ);
                rmem_read(rmem, page, &physmem[i*PAGE_SIZE]);
                pageReads++;
                break;
            }
        }
        // first in first out page replacement
        if (emptyFrame == 0) {
            int value = 0;
            for(i = 0; i < pt->nframes; i++) { // get oldest page
                if(frameState[i] > value) {
                    value = frameState[i];
                    frame = i;
                }
            }
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                rmem_write(rmem, tempPage, &physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            rmem_read(rmem, page, &physmem[frame*PAGE_SIZE]);
            pageReads++;
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            frameState[frame] = 1;
            framePage[frame] = page;
        }
    } else { // if page is already in table--need to set write bit
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
        frameState[frame] = 1;
        framePage[frame] = page;
    }
}

struct disk* disk;
void page_fault_handler_disk( struct page_table *pt, int page) {
    int i;
    int frame;
    int bits;
    
    pageFaults++;

    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = 0; i < pt->nframes; i++) { // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }
    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = 0; i < pt->nframes; i++) {
            if(frameState[i] == 0) { // empty frame
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                page_table_set_entry(pt, page, i, PROT_READ);
                disk_read(disk, page, &physmem[i*PAGE_SIZE]);
                pageReads++;
                break;
            }
        }
        // first in first out page replacement
        if (emptyFrame == 0) {
            int value = 0;
            for(i = 0; i < pt->nframes; i++) { // get oldest page
                if(frameState[i] > value) {
                    value = frameState[i];
                    frame = i;
                }
            }
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                disk_write(disk, tempPage, &physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
            pageReads++;
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            frameState[frame] = 1;
            framePage[frame] = page;
        }
    } else { // if page is already in table--need to set write bit
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
        frameState[frame] = 1;
        framePage[frame] = page;
    }
}

struct mem* mem;
void page_fault_handler_mem( struct page_table *pt, int page ) {
    int i;
    int frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = 0; i < pt->nframes; i++) { // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }

    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = 0; i < pt->nframes; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                page_table_set_entry(pt, page, i, PROT_READ);
                mem_read(mem, page, &physmem[i*PAGE_SIZE]);
                pageReads++;
                break;
            }
        }
        // first in first out page replacement
        if (emptyFrame == 0) {
            int value = 0;
            for(i = 0; i < pt->nframes; i++) { // get oldest page
                if(frameState[i] > value) {
                    value = frameState[i];
                    frame = i;
                }
            }
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                mem_write(mem, tempPage, &physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            mem_read(mem, page, &physmem[frame*PAGE_SIZE]);
            pageReads++;
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            frameState[frame] = 1;
            framePage[frame] = page;
        }
    } else { // if page is already in table--need to set write bit
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
        frameState[frame] = 1;
        framePage[frame] = page;
    }
}


struct page_table *page_table_create( int npages, int nframes, page_fault_handler_t handler ) {
    int i;
    struct sigaction sa;
    struct page_table *pt;

    pt = malloc(sizeof(struct page_table));
    if(!pt) return 0;

    the_page_table = pt;

/*
    char filename[256];
    sprintf(filename,"/tmp/pmem.%d.%d",getpid(),getuid());

    pt->fd = open(filename,O_CREAT|O_TRUNC|O_RDWR,0777);
    if(!pt->fd) return 0;

    ftruncate(pt->fd,PAGE_SIZE*npages);

    unlink(filename);*/

    //pt->physmem = mmap(0,nframes*PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,pt->fd,0);
    pt->physmem = mmap(NULL, nframes*PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (pt->physmem == (void *) MAP_FAILED) perror("mmap"), exit(0);

    pt->nframes = nframes;

    //pt->virtmem = mmap(0,npages*PAGE_SIZE,PROT_NONE,MAP_SHARED|MAP_NORESERVE,pt->fd,0);
    pt->virtmem = mmap(NULL, npages*PAGE_SIZE, PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
    if (pt->virtmem == (void *) MAP_FAILED) perror("mmap"), exit(0);

    pt->npages = npages;

    pt->page_bits = malloc(sizeof(int)*npages);
    pt->page_mapping = malloc(sizeof(int)*npages);

    pt->handler = handler;
    /*printf("The size of our local memory cache is: %d kbyte\n", nframes*PAGE_SIZE/1000);
    printf ("The size of our page table is: %d\n", nframes);
    printf("The size of the virtual memory is: %d kbyte\n", npages*PAGE_SIZE/1000);*/
    for(i=0;i<pt->npages;i++) pt->page_bits[i] = 0;

    sa.sa_sigaction = internal_fault_handler;
    sa.sa_flags = SA_SIGINFO;

    sigfillset( &sa.sa_mask );
    sigaction( SIGSEGV, &sa, 0 );

    return pt;
}

void page_table_delete(struct page_table *pt) {
    munmap(pt->virtmem,pt->npages*PAGE_SIZE);
    munmap(pt->physmem,pt->nframes*PAGE_SIZE);
    free(pt->page_bits);
    free(pt->page_mapping);
    close(pt->fd);
    free(pt);
}

void page_table_flush(struct page_table *pt) {
    int frame;
    int bits;
    for(frame = 0; frame < pt->nframes; frame++) { // get oldest page
        int tempPage = framePage[frame]; // get page we want to kick out
        page_table_get_entry(pt, tempPage, &frame, &bits);
        if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
            if (!strcmp(pagingSystem, "disk"))
                disk_write(disk, tempPage, &physmem[frame*PAGE_SIZE]);
            else if (!strcmp(pagingSystem, "rmem"))
                rmem_write(rmem, tempPage, &physmem[frame*PAGE_SIZE]);
            else if (!strcmp(pagingSystem, "rrmem"))
                rrmem_write(rrmem, tempPage, &physmem[frame*PAGE_SIZE]);
            pageWrites++;
        }
        page_table_set_entry(pt, tempPage, frame, PROT_NONE);
        frameState[frame] = 0;
    }
}

void page_table_set_entry(struct page_table *pt, int page, int frame, int bits ) {
    if( page<0 || page>=pt->npages ) {
        fprintf(stderr,"page_table_set_entry: illegal page #%d\n",page);
        abort();
    }

    if( frame<0 || frame>=pt->nframes ) {
        fprintf(stderr,"page_table_set_entry: illegal frame #%d\n",frame);
        abort();
    }

    pt->page_mapping[page] = frame;
    pt->page_bits[page] = bits;
    remap_file_pages(pt->virtmem+page*PAGE_SIZE,PAGE_SIZE,0,frame,0);
    mprotect(pt->virtmem+page*PAGE_SIZE,PAGE_SIZE,bits);

}

void page_table_get_entry( struct page_table *pt, int page, int *frame, int *bits ) {
    if( page<0 || page>=pt->npages ) {
        fprintf(stderr,"page_table_get_entry: illegal page #%d\n",page);
        abort();
    }

    *frame = pt->page_mapping[page];
    *bits = pt->page_bits[page];
}

void page_table_print_entry( struct page_table *pt, int page ) {
    if( page<0 || page>=pt->npages ) {
        fprintf(stderr,"page_table_print_entry: illegal page #%d\n",page);
        abort();
    }

    int b = pt->page_bits[page];

    printf("page %06d: frame %06d bits %c%c%c\n",
        page,
        pt->page_mapping[page],
        b&PROT_READ  ? 'r' : '-',
        b&PROT_WRITE ? 'w' : '-',
        b&PROT_EXEC  ? 'x' : '-'
    );
}

void page_table_print( struct page_table *pt ) {
    int i;
    for(i=0;i<pt->npages;i++) {
        page_table_print_entry(pt,i);
    }
}

int page_table_get_nframes( struct page_table *pt ) {
    return pt->nframes;
}

int page_table_get_npages( struct page_table *pt ) {
    return pt->npages;
}

char * page_table_get_virtmem( struct page_table *pt ) {
    return pt->virtmem;
}

char * page_table_get_physmem( struct page_table *pt ) {
    return pt->physmem;
}


//TODO make a version of this for rrmem
void set_vmem_config(char *filename) {
    configure_rmem(filename);
}

struct page_table *init_virtual_memory(int npages, int nframes, const char* system) {
    struct page_table *pt;
    pagingSystem = system;
    int i;
    frameState = malloc(sizeof(int[nframes])); // allocate space for array of frameStates
    for(i = 0; i < nframes; i++) {
        frameState[i] = 0;
    }

    framePage = malloc(sizeof(int[nframes])); // allocate space for array of framePages
    for(i = 0; i < nframes; i++) {
        framePage[i] = 0;
    }
    
    if (!strcmp(pagingSystem, "rmem")) {
        rmem = rmem_allocate(npages);
        if(!rmem) {
            fprintf(stderr,"couldn't create virtual rmem: %s\n",strerror(errno));
            abort();
        }
        pt = page_table_create( npages, nframes, page_fault_handler_rmem );
    }
    else if (!strcmp(pagingSystem, "rrmem")) {
        rrmem = rrmem_allocate(npages);
  struct rrmem *rrmem_allocate(int blocks);
        if(!rrmem) {
            fprintf(stderr,"couldn't create virtual rrmem: %s\n",strerror(errno));
            abort();
        }
        pt = page_table_create( npages, nframes, page_fault_handler_rrmem );
    }
    else if (!strcmp(pagingSystem, "disk")) {
        disk = disk_open("myvirtualdisk",npages);
        if(!disk) {
            fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
            abort();
        }
        pt = page_table_create( npages, nframes, page_fault_handler_disk );
    } else if (!strcmp(pagingSystem, "mem")) {
        mem = mem_allocate(npages);
        if(!mem) {
            fprintf(stderr,"couldn't create virtual mem: %s\n",strerror(errno));
            abort();
        }
        pt = page_table_create( npages, nframes, page_fault_handler_mem );
    }
    else {
        perror("Please specify the correct page table structure (rrmem|rmem|disk)!");
        abort();
    }

    if(!pt) {
        fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
        abort();
    }
    physmem = page_table_get_physmem(pt);
    return pt;
}

void print_page_faults() {
    printf("page faults: %d     rmem reads: %d     rmem writes: %d\n", pageFaults, pageReads, pageWrites);
}

void clean_page_table(struct page_table *pt) {
    free(frameState);
    free(framePage);
/*    free(the_page_table);
    free(physmem);*/
    page_table_delete(pt);
    if (!strcmp(pagingSystem, "disk"))
        disk_close(disk);
    else if (!strcmp(pagingSystem, "rmem"))
        rmem_deallocate(rmem);
    else if (!strcmp(pagingSystem, "rrmem"))
        rrmem_deallocate(rrmem);
    pageFaults = 0;
    pageReads = 0;
    pageWrites = 0;
}
