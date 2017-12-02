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
#include <time.h>

static int *frameState;     // keeps track of how long a page has been in a frame
static int *framePage;      // keeps track of which page is in a frame
static struct page_table *the_page_table = 0;
static __thread struct hash *hashTable = NULL;
static __thread struct lruList *lruList = NULL;
static __thread struct freqList *freqList = NULL;
static int pageFaults = 0;  // statistics 
static int pageReads = 0;   // statistics 
static int pageWrites = 0;  // statistics 
static int pagingSystem;
static int replacementPolicy;
static const int MEM_PAGING = 0;
static const int RMEM_PAGING = 1;
static const int DISK_PAGING = 2;
static const int RRMEM_PAGING = 3;
static const int FIFO = 0;
static const int LRU = 1;
static const int LFU = 2;
static const int RAND = 3;
static __thread int thread_id;
static __thread int startFrame;
static __thread int endFrame;


struct mem *mem;
struct disk *disk;
struct rmem *rmem;
struct rrmem *rrmem;

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

void LRU_page_fault_handler( struct page_table *pt, int page ) {
    int frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    if(bits == 0) { // page is not in memory
        if(lruList->count < pt->nframes)
        {
            int i = lruList->count + startFrame;
            insertHashNode(page,createlruListNode(page));
            page_table_set_entry(pt, page, i, PROT_READ);
            if (pagingSystem == MEM_PAGING)
                mem_read(mem, page, &pt->physmem[i*PAGE_SIZE]);
            else if (pagingSystem == RMEM_PAGING)
                rmem_read(rmem, page, &pt->physmem[i*PAGE_SIZE]);
            else if (pagingSystem == DISK_PAGING)
                disk_read(disk, page, &pt->physmem[i*PAGE_SIZE]);
            else if (pagingSystem == RRMEM_PAGING)
                 rrmem_read(rrmem, page, &pt->physmem[i*PAGE_SIZE]);
            pageReads++;
        }
        else
        {
            int tempPage;
            deleteLRU(&tempPage);
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                if (pagingSystem == MEM_PAGING)
                    mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RMEM_PAGING)
                    rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == DISK_PAGING)
                    disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RRMEM_PAGING)
                    rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            if (pagingSystem == MEM_PAGING)
                mem_read(mem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RMEM_PAGING)
                rmem_read(rmem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == DISK_PAGING)
                disk_read(disk, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RRMEM_PAGING)
                rrmem_read(rrmem, page, &pt->physmem[frame*PAGE_SIZE]);
            pageReads++;
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            insertHashNode(page,createlruListNode(page));
        }
    } else { // if page is already in table--need to set write bit
        moveListNodeToFront(getHashNode(page));
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
    }

    /*struct lruListNode *temp = lruList->head;
    for(int i = 0; i < lruList->count; i++)
    {
        printf("%d, ",temp->key);
        temp = temp->next;
    }
    printf("\n\n");*/
}

void LFU_page_fault_handler( struct page_table *pt, int page ) {
    int frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    if(bits == 0) { // page is not in memory
        if(freqList->count < pt->nframes)
        {
            int i = freqList->count + startFrame;
            insertHashNode(page,createlfuListNode(page));
            page_table_set_entry(pt, page, i, PROT_READ);
            if (pagingSystem == MEM_PAGING)
                mem_read(mem, page, &pt->physmem[i*PAGE_SIZE]);
            else if (pagingSystem == RMEM_PAGING)
                rmem_read(rmem, page, &pt->physmem[i*PAGE_SIZE]);
            else if (pagingSystem == DISK_PAGING)
                disk_read(disk, page, &pt->physmem[i*PAGE_SIZE]);
            else if (pagingSystem == RRMEM_PAGING)
                rrmem_read(rrmem, page, &pt->physmem[i*PAGE_SIZE]);
            pageReads++;
        }
        else
        {
            int tempPage;
            deleteLFU(&tempPage);
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                if (pagingSystem == MEM_PAGING)
                    mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RMEM_PAGING)
                    rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == DISK_PAGING)
                    disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RRMEM_PAGING)
                    rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            if (pagingSystem == MEM_PAGING)
                mem_read(mem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RMEM_PAGING)
                rmem_read(rmem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == DISK_PAGING)
                disk_read(disk, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RRMEM_PAGING)
                rrmem_read(rrmem, page, &pt->physmem[frame*PAGE_SIZE]);

            pageReads++;
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            insertHashNode(page,createlfuListNode(page));
        }
    } else { // if page is already in table--need to set write bit
        moveNodeToNextFreq(getHashNode(page));
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
    }

    /*struct freqListNode *fListnode = freqList->head;
    int i = 0;
    if (fListnode != NULL)
        while (fListnode != NULL)
        {
            printf("index: %d | use cnt %d:", i, fListnode->useCount);
            struct lfuListNode *lfuListNode = fListnode->head;
            while (lfuListNode != NULL)
            {
                printf("%d, ", lfuListNode->key);
                lfuListNode = lfuListNode->next;
            }
            fListnode = fListnode->next;
            i++;
            printf("\n");
        }
    printf("\n\n");*/
}

void FIFO_page_fault_handler( struct page_table *pt, int page ) {
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
                page_table_set_entry(pt, page, i, PROT_READ);

                if (pagingSystem == MEM_PAGING)
                    mem_read(mem, page, &pt->physmem[i*PAGE_SIZE]);
                else if (pagingSystem == RMEM_PAGING)
                    rmem_read(rmem, page, &pt->physmem[i*PAGE_SIZE]);
                else if (pagingSystem == DISK_PAGING)
                    disk_read(disk, page, &pt->physmem[i*PAGE_SIZE]);
                else if (pagingSystem == RRMEM_PAGING)
                    rrmem_read(rrmem, page, &pt->physmem[i*PAGE_SIZE]);
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
                if (pagingSystem == MEM_PAGING)
                    mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RMEM_PAGING)
                    rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == DISK_PAGING)
                    disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RRMEM_PAGING)
                    rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }

            if (pagingSystem == MEM_PAGING)
                mem_read(mem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RMEM_PAGING)
                rmem_read(rmem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == DISK_PAGING)
                disk_read(disk, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RRMEM_PAGING)
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

void RAND_page_fault_handler( struct page_table *pt, int page ) {
    int i;
    int frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    if(bits == 0) { // page is not in memory
        int emptyFrame = 0;
        for(i = startFrame; i < endFrame; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                page_table_set_entry(pt, page, i, PROT_READ);

                if (pagingSystem == MEM_PAGING)
                    mem_read(mem, page, &pt->physmem[i*PAGE_SIZE]);
                else if (pagingSystem == RMEM_PAGING)
                    rmem_read(rmem, page, &pt->physmem[i*PAGE_SIZE]);
                else if (pagingSystem == DISK_PAGING)
                    disk_read(disk, page, &pt->physmem[i*PAGE_SIZE]);
                else if (pagingSystem == RRMEM_PAGING)
                    rrmem_read(rrmem, page, &pt->physmem[i*PAGE_SIZE]);
                pageReads++;
                break;
            }
        }
        // randomly remove a page
        if (emptyFrame == 0) {
            frame = (rand() % pt->nframes) + startFrame;
            int tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                if (pagingSystem == MEM_PAGING)
                    mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RMEM_PAGING)
                    rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == DISK_PAGING)
                    disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RRMEM_PAGING)
                    rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }

            if (pagingSystem == MEM_PAGING)
                mem_read(mem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RMEM_PAGING)
                rmem_read(rmem, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == DISK_PAGING)
                disk_read(disk, page, &pt->physmem[frame*PAGE_SIZE]);
            else if (pagingSystem == RRMEM_PAGING)
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


struct page_table *init_virtual_memory(int npages, int nframes, const char* system, const char* algo) {
    struct page_table *pt;
    if (!strcmp(system, "mem"))
        pagingSystem = MEM_PAGING;
    else if (!strcmp(system, "rmem"))
        pagingSystem = RMEM_PAGING;
    else if (!strcmp(system, "disk"))
        pagingSystem = DISK_PAGING;
    else if (!strcmp(system, "rrmem"))
        pagingSystem = RRMEM_PAGING;
    else {
        perror("Please specify the correct page table structure (rmem|disk|mem|rrmem)!!");
        abort();
    }

    if (!strcmp(algo, "fifo"))
    	replacementPolicy = FIFO;
    else if (!strcmp(algo, "lru"))
    	replacementPolicy = LRU;
    else if (!strcmp(algo, "lfu"))
    	replacementPolicy = LFU;
    else if (!strcmp(algo, "rand"))
    	replacementPolicy = RAND;
    else
    {
    	perror("Please specify the correct page replacement policy (fifo|lru|lfu|rand)!!");
        abort();
    }
    
    if (pagingSystem == RMEM_PAGING) {
        rmem = rmem_allocate(npages);
        if(!rmem) {
            fprintf(stderr,"couldn't create virtual rmem: %s\n",strerror(errno));
            abort();
        }
    }
    else if (pagingSystem == DISK_PAGING) {
        disk = disk_open("myvirtualdisk",npages);
        if(!disk) {
            fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
            abort();
        }
    } else if (pagingSystem == MEM_PAGING) {
        mem = mem_allocate(npages);
        if(!mem) {
            fprintf(stderr,"couldn't create virtual mem: %s\n",strerror(errno));
            abort();
        }
    } 
    else if(pagingSystem == RRMEM_PAGING)
    {
        rrmem = rrmem_allocate(npages);
        if(!rrmem) {
            fprintf(stderr,"couldn't create virtual rrmem: %s\n",strerror(errno));
            abort();
        }
    }

    if(replacementPolicy == FIFO)
    {
        int i;
        frameState = calloc(nframes, sizeof(int)); // allocate space for array of frameStates
        for(i = 0; i < nframes; i++) 
            frameState[i] = 0;
        framePage = calloc(nframes, sizeof(int)); // allocate space for array of framePages
        for(i = 0; i < nframes; i++) 
            framePage[i] = 0;
        pt = page_table_create( npages, nframes, FIFO_page_fault_handler );
    }
    else if(replacementPolicy == LRU)
    {
    	hashTable = (struct hash *) calloc(nframes, sizeof(struct hash));
        lruList = (struct lruList*) calloc(nframes, sizeof(struct lruListNode));
        lruList->count = 0;
        pt = page_table_create( npages, nframes, LRU_page_fault_handler );
    }
    else if(replacementPolicy == LFU)
    {
        hashTable = (struct hash *) calloc(nframes, sizeof(struct hash));
        freqList = (struct freqList*) calloc(nframes, sizeof(struct lfuListNode));
        freqList->count = 0;
        pt = page_table_create( npages, nframes, LFU_page_fault_handler );
    }
    else if(replacementPolicy == RAND)
    {
        int i;
        frameState = calloc(nframes, sizeof(int)); // allocate space for array of frameStates
        for(i = 0; i < nframes; i++) 
            frameState[i] = 0;
        framePage = calloc(nframes, sizeof(int)); // allocate space for array of framePages
        for(i = 0; i < nframes; i++) 
            framePage[i] = 0;
        pt = page_table_create( npages, nframes, RAND_page_fault_handler );
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
    if(replacementPolicy == LRU)
    {
        hashTable = (struct hash *) calloc(pt->nframes, sizeof(struct hash));
        lruList = (struct lruList*) calloc(pt->nframes, sizeof(struct lruListNode));
        lruList->count = 0;
    }
    else if(replacementPolicy == LFU)
    {
        hashTable = (struct hash *) calloc(pt->nframes, sizeof(struct hash));
        freqList = (struct freqList*) calloc(pt->nframes, sizeof(struct lfuListNode));
        freqList->count = 0;
    }
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
    if (replacementPolicy == FIFO || replacementPolicy == RAND)
    {
    	frameState = realloc(frameState, pt->nframes*num_threads*sizeof(int));
    	if (!frameState) perror("realloc"), free(frameState), exit(0);
    	framePage = realloc(framePage, pt->nframes*num_threads*sizeof(int));
    	if (!framePage) perror("realloc"), free(framePage), exit(0);
    	memset(frameState, 0, pt->nframes*num_threads*sizeof(int));
    	memset(framePage, 0, pt->nframes*num_threads*sizeof(int));
    }
}

struct page_table *page_table_create(int npages, int nframes, page_fault_handler_t handler) {
    struct sigaction sa;
    struct page_table *pt;
    uint64_t page_space = PAGE_SIZE * (uint64_t) npages;
    uint64_t frame_space = PAGE_SIZE * (uint64_t) nframes;

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
    if(replacementPolicy == FIFO || replacementPolicy == RAND)
    {
        for(int i = 0; i < pt->nframes; i++) { 
            int tempPage = framePage[i]; // get page we want to kick out 
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                if (pagingSystem == DISK_PAGING)
                    disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RMEM_PAGING)
                    rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == MEM_PAGING)
                    mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                else if (pagingSystem == RRMEM_PAGING)
                    rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                pageWrites++;
            }
            page_table_set_entry(pt, tempPage, frame, PROT_NONE);
            frameState[frame] = 0;
        }
    }
    else if(replacementPolicy == LRU)
    {
        struct hashNode *currNode;
        for(int i = 0; i < pt->nframes; i++) {
            if (hashTable[i].count == 0)
                continue;
            currNode = hashTable[i].head;
            if (!currNode)
                continue;
            while (currNode != NULL) {
                int tempPage = currNode->key;
                page_table_get_entry(pt, tempPage, &frame, &bits);

                if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                    if (pagingSystem == DISK_PAGING)
                        disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    else if (pagingSystem == RMEM_PAGING)
                        rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    else if (pagingSystem == MEM_PAGING)
                        mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    else if (pagingSystem == RRMEM_PAGING)
                        rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    pageWrites++;
                }
                int temp;
                deleteLRU(&temp);
                page_table_set_entry(pt, tempPage, frame, PROT_NONE);
                currNode = currNode->next;
            }
        }
    }
    else if(replacementPolicy == LFU)
    {
        struct hashNode *currNode;
        for(int i = 0; i < pt->nframes; i++) {
            if (hashTable[i].count == 0)
                continue;
            currNode = hashTable[i].head;
            if (!currNode)
                continue;
            while (currNode != NULL) {
                int tempPage = currNode->key;
                page_table_get_entry(pt, tempPage, &frame, &bits);

                if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                    if (pagingSystem == DISK_PAGING)
                        disk_write(disk, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    else if (pagingSystem == RMEM_PAGING)
                        rmem_write(rmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    else if (pagingSystem == MEM_PAGING)
                        mem_write(mem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    else if (pagingSystem == RRMEM_PAGING)
                        rrmem_write(rrmem, tempPage, &pt->physmem[frame*PAGE_SIZE]);
                    pageWrites++;
                }
                page_table_set_entry(pt, tempPage, frame, PROT_NONE);
                int temp;
                deleteLFU(&temp);
                currNode = currNode->next;
            }
        }
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
    printf("page faults: %d     reads: %d     writes: %d\n", pageFaults, pageReads, pageWrites);
}

void clean_page_table(struct page_table *pt) {
    free(hashTable);
    free(lruList);
    free(freqList);
    free(framePage);
    free(frameState);
    page_table_delete(pt);
    if (pagingSystem == DISK_PAGING)
        disk_close(disk);
    else if (pagingSystem == RMEM_PAGING)
        rmem_deallocate(rmem);
    else if (pagingSystem == RRMEM_PAGING)
        rrmem_deallocate(rrmem);
    pageFaults = 0;
    pageReads = 0;
    pageWrites = 0;
}


struct lruListNode *createlruListNode(int key)
{
    struct lruListNode *newnode;
    newnode = (struct lruListNode *) malloc(sizeof(struct lruListNode));
    newnode->key = key;
    newnode->prev = NULL;
    newnode->next = lruList->head;
    if (lruList->count > 0)
    {
        lruList->head->prev = newnode;
    }
    else
        lruList->tail = newnode;
    lruList->head = newnode;
    lruList->count++;
    return newnode;
}

struct lfuListNode *createlfuListNode(int key)
{
    struct lfuListNode *newnode;
    newnode = (struct lfuListNode *) malloc(sizeof(struct lfuListNode));
    newnode->key = key;
    newnode->next = NULL;
    newnode->prev = NULL;
    if (freqList->head != NULL && freqList->head->useCount == 0)
    {
        newnode->next = freqList->head->head;
        freqList->head->head->prev = newnode;
        freqList->head->head = newnode;
    }
    else
    {
        struct freqListNode *newFreqNode;
        newFreqNode = (struct freqListNode *) malloc(sizeof(struct freqListNode));
        newFreqNode->useCount = 0;
        newFreqNode->head = newnode;
        newFreqNode->tail = newnode;
        newFreqNode->next = NULL;
        newFreqNode->prev = NULL;
        if (freqList->head != NULL)
        {
            freqList->head->prev = newFreqNode;
            newFreqNode->next = freqList->head;
        }
        freqList->head = newFreqNode;
    }
    newnode->parent = freqList->head;
    freqList->count++;
    return newnode;
}

void moveListNodeToFront(struct lruListNode *node)
{
    if(lruList->head == node)
        return;
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    node->next = lruList->head;
    if(lruList-> tail == node)
        lruList->tail = node->prev;
    node->prev = NULL;
    if (lruList->count > 1)
    
        lruList->head->prev = node;
    else
    {
        lruList->head = node;
        lruList->tail = node;
    }
    lruList->head = node;
}

void moveNodeToNextFreq(struct lfuListNode *node)
{
    if (node->parent->next == NULL) //Check if the next freq exists, if not make it
    {
        struct freqListNode *newFreqNode;
        newFreqNode = (struct freqListNode *) malloc(sizeof(struct freqListNode));
        newFreqNode->useCount = node->parent->useCount+1;
        newFreqNode->prev = node->parent;
        node->parent->next = newFreqNode;
        newFreqNode->head = NULL;
        newFreqNode->tail = NULL;
        newFreqNode->next = NULL;
        newFreqNode->prev = node->parent;

    }
    else if (node->parent->next->useCount > node->parent->useCount + 1) //check if next frequency is +1
    {
        struct freqListNode *newFreqNode;
        newFreqNode = (struct freqListNode *) malloc(sizeof(struct freqListNode));
        newFreqNode->useCount = node->parent->useCount + 1;
        newFreqNode->head = NULL;
        newFreqNode->tail = NULL;
        node->parent->next->prev = newFreqNode;
        newFreqNode->next = node->parent->next;
        node->parent->next = newFreqNode;
        newFreqNode->prev = node->parent;
    }

    if (node->prev != NULL) //Move node
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    if (node->parent->head == node)
        node->parent->head = node->next;
    if (node->parent->tail == node)
        node->parent->tail = node->prev;

    struct freqListNode *temp = node->parent;
    node->parent = node->parent->next;
    if (node->parent->head != NULL)
        node->parent->head->prev = node;
    node->next = node->parent->head;
    node->prev = NULL;
    node->parent->head = node;
    if (node->parent->tail == NULL)
        node->parent->tail = node;

    if (temp->head == NULL)
    {
        if (temp == freqList->head)
            freqList->head = temp->next;
        if (temp->prev != NULL)
            temp->prev->next = temp->next;
        if (temp->next != NULL)
            temp->next->prev = temp->prev;
        free(temp);
    }
}


void deletelruListNode(struct lruListNode *node)
{
    if (node == lruList->head)
        lruList->head = node->next;
    if (node == lruList->tail)
        lruList->tail = node->prev;
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    lruList->count--;
    free(node);
}

void deletelfuListNode(struct lfuListNode *node)
{
    if (node == node->parent->head)
        node->parent->head = node->next;
    if (node == node->parent->tail)
        node->parent->tail = node->prev;
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    if (node->parent->head == NULL)
    {
        if (node->parent == freqList->head)
            freqList->head = node->parent->next;
        node->parent->prev = node->parent->next;
        node->parent->next = node->parent->prev;
    }
    free(node);
    freqList->count--;
}

struct hashNode * createHashNode(int key) {
    struct hashNode *newnode;
    newnode = (struct hashNode *) malloc(sizeof(struct hashNode));
    newnode->key = key;
    newnode->next = NULL;
    return newnode;
}

int hashFunction(int key)
{
    return key % the_page_table->nframes;
}

void insertHashNode(int key, void* listnodePointer) {
    int hashIndex = hashFunction(key);
    struct hashNode *newnode = createHashNode(key);
    newnode->listNodePointer = listnodePointer;
    // head of list for the bucket with index hashIndex
    if (!hashTable[hashIndex].head) {
        hashTable[hashIndex].head = newnode;
        hashTable[hashIndex].count = 1;
        return;
    }
    // adding new node to the list
    newnode->next = (hashTable[hashIndex].head);
    // update the head of the list and none of nodes in the current bucket
    hashTable[hashIndex].head = newnode;
    hashTable[hashIndex].count++;
    return;
}

void deleteHashNode(int key) {
    // find the bucket using hash index
    int hashIndex = hashFunction(key);
    struct hashNode *temp, *myNode;
    // get the list head from current bucket
    myNode = hashTable[hashIndex].head;
    if (!myNode)
        return;
    temp = myNode;
    while (myNode != NULL) {
        // delete the node with given key
        if (myNode->key == key) {
            if (myNode == hashTable[hashIndex].head)
                hashTable[hashIndex].head = myNode->next;
            else
                temp->next = myNode->next;

            hashTable[hashIndex].count--;
            free(myNode);
            break;
        }
        temp = myNode;
        myNode = myNode->next;
    }
}

void* getHashNode(int key) {
    int hashIndex = hashFunction(key);
    struct hashNode *myNode;
    myNode = hashTable[hashIndex].head;
    if (!myNode) 
        return NULL;
    while (myNode != NULL) {
        if (myNode->key == key)
            break;
        myNode = myNode->next;
    }
    return myNode->listNodePointer;
}

void deleteLRU(int *page)
{
    *page = -1;
    if (lruList->tail != NULL)
    {
        *page = lruList->tail->key;
        deleteHashNode(lruList->tail->key);
        deletelruListNode(lruList->tail);
    }
}

void deleteLFU(int *page)
{
    *page = -1;
    if (freqList->head != NULL)
    {
        *page = freqList->head->tail->key;
        deleteHashNode(freqList->head->tail->key);
        deletelfuListNode(freqList->head->tail);
    }
}