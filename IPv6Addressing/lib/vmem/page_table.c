#define _GNU_SOURCE

#include "page_table.h"
#include "rmem.h"
#include "rrmem.h"
#include "disk.h"
#include "mem.h"
#include "../utils.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

static int *frameState;     // keeps track of how long a page has been in a frame
static int *framePage;      // keeps track of which page is in a frame
static int pageFaults = 0;  // statistics 
static int pageReads = 0;   // statistics 
static int pageWrites = 0;  // statistics 
static const char *pagingSystem;
static struct page_table *the_page_table = 0;
static __thread int thread_id;
static __thread int startFrame;
static __thread int endFrame;


static void internal_fault_handler(int signum, siginfo_t *info, void *context) {
    (void) signum;
    char *addr = info->si_addr;
    (void) context;
    struct page_table *pt = the_page_table;
    if(pt) {
        int page = (addr-pt->virtmem) / PAGE_SIZE;
        //printf("\x1B[3%dm""Thread %d Faulting at %p and page %d \n"RESET,thread_id+1,thread_id, addr, page);
        if(page>=0 && page<pt->npages) {
            pt->handler(pt, page);
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
                rrmem_read(rrmem, page, &pt->physmem[i*PAGE_SIZE]);
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
            //printf("Physical Mem\n");
            //printNBytes(&physmem[frame*PAGE_SIZE],PAGE_SIZE);
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            rrmem_read(rrmem, page, &pt->physmem[frame*PAGE_SIZE]);
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
struct rmem *rmem;
void page_fault_handler_rmem(struct page_table *pt, int page) {
    int i;
    int frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = startFrame; i < endFrame; i++) {
    // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }
    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = startFrame; i < endFrame; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                page_table_set_entry(pt, page, i, PROT_READ);
                rmem_read(rmem, page, &pt->physmem[i * PAGE_SIZE]);
                pageReads++;
                break;
            }
        }
        // first in first out page replacement
        if (emptyFrame == 0) {
            int value = 0;
            for(i = startFrame; i < endFrame; i++) { // get oldest page
                if(frameState[i] > value) {
                    value = frameState[i];
                    frame = i;
                }
            }
            int tempPage = framePage[frame]; // get page currently in frame

            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                rmem_write(rmem, tempPage, &pt->physmem[frame * PAGE_SIZE]);
                pageWrites++;
            }
            rmem_read(rmem, page, &pt->physmem[frame * PAGE_SIZE]);
            pageReads++;
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, PROT_NONE);
            frameState[frame] = 1;
            framePage[frame] = page;
        }
    } else { // if page is already in table--need to set write bit
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
        frameState[frame] = 1;
        framePage[frame] = page;
    }
}

struct disk *disk;
void page_fault_handler_disk(struct page_table *pt, int page) {
    int i;
    int frame;
    int bits;
    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = startFrame; i < endFrame; i++) { // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }
    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = startFrame; i < endFrame; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                //printf("\x1B[3%dm""Thread %d Insert page %d at frame %d \n"RESET,thread_id+1, thread_id, page, i);
                page_table_set_entry(pt, page, i, PROT_READ);
                disk_read(disk, page, &pt->physmem[i*PAGE_SIZE]);
                pageReads++;

                break;
            }
        }
        // first in first out page replacement
        if (emptyFrame == 0) {
            int value = 0;
            for(i = startFrame; i < endFrame; i++) { // get oldest page
                if(frameState[i] > value) {
                    value = frameState[i];
                    frame = i;
                }
            }
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }

            disk_read(disk, page, &pt->physmem[frame*PAGE_SIZE]);
            pageReads++;
            //printf("\x1B[3%dm""Thread %d Evict page %d at frame %d for page %d \n"RESET,thread_id+1, thread_id, tempPage, frame, page);
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, PROT_NONE);


            frameState[frame] = 1;
            framePage[frame] = page;
        }
    } else { // if page is already in table--need to set write bit
        //printf("\x1B[3%dm""Thread %d Wrote to page %d \n"RESET,thread_id+1, thread_id, page);
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
        frameState[frame] = 1;
        framePage[frame] = page;
    }
}

struct mem *mem;
void page_fault_handler_mem(struct page_table *pt, int page) {
    int i;
    int frame;
    int bits;
    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = startFrame; i < endFrame; i++) { // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }
    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = startFrame; i < endFrame; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                //printf("\x1B[3%dm""Thread %d Insert page %d at frame %d \n"RESET,thread_id+1, thread_id, page, i);
                page_table_set_entry(pt, page, i, PROT_READ);
                mem_read(mem, page, &pt->physmem[i*PAGE_SIZE]);
                pageReads++;

                break;
            }
        }
        // first in first out page replacement
        if (emptyFrame == 0) {
            int value = 0;
            for(i = startFrame; i < endFrame; i++) { // get oldest page
                if(frameState[i] > value) {
                    value = frameState[i];
                    frame = i;
                }
            }
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }

            mem_read(mem, page, &pt->physmem[frame*PAGE_SIZE]);
            pageReads++;
            //printf("\x1B[3%dm""Thread %d Evict page %d at frame %d for page %d \n"RESET,thread_id+1, thread_id, tempPage, frame, page);
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, PROT_NONE);


            frameState[frame] = 1;
            framePage[frame] = page;
        }
    } else { // if page is already in table--need to set write bit
        //printf("\x1B[3%dm""Thread %d Wrote to page %d \n"RESET,thread_id+1, thread_id, page);
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
        frameState[frame] = 1;
        framePage[frame] = page;
    }
}

struct page_table *init_virtual_memory(int npages, int nframes, const char* system) {
    struct page_table *pt;
    pagingSystem = system;
    frameState = calloc(nframes, sizeof(int));// allocate space for array of frameStates
    framePage = calloc(nframes, sizeof(int)); // allocate space for array of framePages

    if (!strcmp(pagingSystem, "rmem")) {
        rmem = rmem_allocate(npages);
        if(!rmem) {
            fprintf(stderr,"couldn't create virtual rmem: %s\n",strerror(errno));
            abort();
        }
        pt = page_table_create(npages, nframes, page_fault_handler_rmem);
    }
    else if (!strcmp(pagingSystem, "disk")) {
        disk = disk_open("myvirtualdisk", npages);
        if(!disk) {
            fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
            abort();
        }
        pt = page_table_create(npages, nframes, page_fault_handler_disk);
    } else if (!strcmp(pagingSystem, "mem")) {
        mem = mem_allocate(npages);
        if(!mem) {
            fprintf(stderr,"couldn't create virtual mem: %s\n",strerror(errno));
            abort();
        }
        pt = page_table_create(npages, nframes, page_fault_handler_mem);
    }
    else {
        perror("Please specify the correct page table structure (rmem|disk)!");
        abort();
    }

    if(!pt) {
        fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
        abort();
    }
    return pt;
}

void init_vmem_thread(int t_id) {
    thread_id = t_id;
    struct page_table *pt = the_page_table;
    startFrame = (t_id) * pt->nframes;
    endFrame = (t_id + 1) * pt->nframes;
    printf("\x1B[3%dm""Thread %d Start Frame %d End Frame %d \n"RESET,t_id+1, t_id, startFrame, endFrame);
}

void register_vmem_threads(int num_threads) {
    page_table_flush();
    struct page_table *pt = the_page_table;
    uint64_t frame_space = PAGE_SIZE * (uint64_t) pt->nframes;
    if (frame_space * num_threads > PAGE_SIZE * (uint64_t) pt->npages)
        printf("Error: Cannot support aggregate frame cache larger than page table\n"), exit(0);
    pt->physmem = mremap(pt->physmem, frame_space,
                frame_space*num_threads, MREMAP_MAYMOVE);
    if (pt->physmem == (void *) MAP_FAILED) perror("mmap"), exit(0);
    //int err = posix_fallocate(pt->fd, pt->nframes*PAGE_SIZE, pt->nframes*PAGE_SIZE * (n_threads));
    //if (err < 0) perror("fallocate"), exit(0);
    frameState = realloc(frameState, pt->nframes*num_threads*sizeof(int));
    if (!frameState) perror("realloc"), free(frameState), exit(0);
    framePage = realloc(framePage, pt->nframes*num_threads*sizeof(int));
    if (!framePage) perror("realloc"), free(framePage), exit(0);
    memset(frameState, 0, pt->nframes*num_threads*sizeof(int));
    memset(framePage, 0, pt->nframes*num_threads*sizeof(int));
}

struct page_table *page_table_create(int npages, int nframes, page_fault_handler_t handler) {
    struct sigaction sa;
    struct page_table *pt;
    printf("%d \n", npages );
    uint64_t page_space = PAGE_SIZE * (uint64_t) npages;
    uint64_t frame_space = PAGE_SIZE * (uint64_t) nframes;

    printf("%lu \n", page_space );

    pt = malloc(sizeof(struct page_table));
    if(!pt) return 0;

    the_page_table = pt;
    char filename[256];
    sprintf(filename,"/pmem.%d.%d",getpid(),getuid());
    pt->fd = shm_open(filename,O_CREAT|O_RDWR,0777);
    if(!pt->fd) return 0;

    ftruncate(pt->fd, page_space);

    //unlink(filename);
    pt->physmem = mmap64(0,frame_space,PROT_READ|PROT_WRITE,MAP_SHARED, pt->fd, 0);

    if (pt->physmem == (void *) MAP_FAILED) perror("frame mmap"), exit(0);
    pt->nframes = nframes;
    startFrame = 0;
    endFrame = nframes;
    pt->virtmem = mmap64(0, page_space, PROT_NONE, MAP_SHARED|MAP_NORESERVE, pt->fd, 0);
    if (pt->virtmem == (void *) MAP_FAILED) perror("page mmap"), exit(0);

    frameState = calloc(pt->nframes, sizeof(int)); // allocate space for array of frameStates
    framePage = calloc(pt->nframes, sizeof(int)); // allocate space for array of frameStates
    pt->npages = npages;

    pt->page_bits = calloc(npages, sizeof(int));
    pt->page_mapping = calloc(npages, sizeof(int));
    //printf("Allocations %p %p %p %p %p %p \n", pt->virtmem, pt->physmem, frameState, framePage, pt->page_bits, pt->page_mapping);
    pt->handler = handler;
    sa.sa_sigaction = internal_fault_handler;
    sa.sa_flags = SA_SIGINFO;

    sigfillset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, 0);
    return pt;
}

void page_table_flush() {
    struct page_table *pt = the_page_table;
    int frame;
    int bits;
    for(int i = 0; i < pt->nframes; i++) {
        int tempPage = framePage[i]; // get page we want to kick out
        page_table_get_entry(pt, tempPage, &frame, &bits);
        if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
            if (!strcmp(pagingSystem, "disk"))
                disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
            else if (!strcmp(pagingSystem, "rmem"))
                rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
            else if (!strcmp(pagingSystem, "mem"))
                mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
            else if (!strcmp(pagingSystem, "rrmem"))
                rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
            pageWrites++;
        }
        page_table_set_entry(pt, tempPage, frame, PROT_NONE);
        frameState[frame] = 0;
    }
}

void page_table_delete(struct page_table *pt) {
    munmap(pt->virtmem,pt->npages*PAGE_SIZE);
    munmap(pt->physmem,pt->nframes*PAGE_SIZE);
    free(pt->page_bits);
    free(pt->page_mapping);
    close(pt->fd);
    free(pt);
}

void page_table_set_entry(struct page_table *pt, int page, int frame, int bits) {
    if(page<0 || page>=pt->npages) {
        fprintf(stderr,"page_table_set_entry: illegal page #%d\n",page);
        abort();
    }

    if(frame<startFrame || frame>=endFrame) {
        fprintf(stderr,"page_table_set_entry: illegal frame #%d\n",frame);
        abort();
    }
    pt->page_mapping[page] = frame;
    pt->page_bits[page] = bits;

    remap_file_pages(pt->virtmem+page*PAGE_SIZE, PAGE_SIZE, 0, frame, 0);
    mprotect(pt->virtmem+page*PAGE_SIZE, PAGE_SIZE, bits);
}

void page_table_get_entry(struct page_table *pt, int page, int *frame, int *bits) {
    if(page<0 || page>=pt->npages) {
        fprintf(stderr,"page_table_get_entry: illegal page #%d\n",page);
        abort();
    }
    *frame = pt->page_mapping[page];
    *bits = pt->page_bits[page];
}

void page_table_print_entry(struct page_table *pt, int page) {
    if(page<0 || page>=pt->npages) {
        fprintf(stderr,"page_table_print_entry: illegal page #%d\n",page);
        abort();
    }

    int b = pt->page_bits[page];
    printf("page %06d: frame %06d bits %c%c%c, vaddr %p\n",
        page,
        pt->page_mapping[page],
        b&PROT_READ  ? 'r' : '-',
        b&PROT_WRITE ? 'w' : '-',
        b&PROT_EXEC  ? 'x' : '-',
        &pt->virtmem[page*PAGE_SIZE]
   );
}

void page_table_print() {
    struct page_table *pt = the_page_table;
    int i;
    for(i=0;i<pt->npages;i++) {
        page_table_print_entry(pt,i);
    }
}
void frame_table_print_entry(struct page_table *pt, int frame) {
    printf("frame %06d: page %06d state %d, vaddr %p\n",
        frame,
        framePage[frame],
        frameState[frame],
        &pt->physmem[frame*PAGE_SIZE]
   );
}

void frame_table_print(int num_threads) {
    if (!num_threads) num_threads =1;
    struct page_table *pt = the_page_table;
    int i;
    for(i=0;i<pt->nframes*num_threads;i++) {
        frame_table_print_entry(pt, i);
    }
}

int page_table_get_nframes(struct page_table *pt) {
    return pt->nframes;
}

int page_table_get_npages(struct page_table *pt) {
    return pt->npages;
}

char * page_table_get_virtmem(struct page_table *pt) {
    return pt->virtmem;
}

char *page_table_get_physmem(struct page_table *pt) {
    return pt->physmem;
}


//TODO make a version of this for rrmem
void set_vmem_config(char *filename) {
    configure_rmem(filename);
}

void print_page_faults() {
    printf("page faults: %d     rmem reads: %d     rmem writes: %d\n", pageFaults, pageReads, pageWrites);
}

void clean_page_table(struct page_table *pt) {
    free(frameState);
    free(framePage);
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
