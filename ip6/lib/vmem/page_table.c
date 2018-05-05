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
#include <signal.h>
static int *frameState;     // keeps track of how long a page has been in a frame
static uint64_t *framePage;      // keeps track of which page is in a frame
static struct page_table *the_page_table = 0;
static __thread struct hash *hashTable = NULL;
static __thread struct dllList *dllList = NULL;
static __thread struct freqList *freqList = NULL;
static uint64_t pageFaults = 0;  // statistics 
static uint64_t pageReads = 0;   // statistics 
static uint64_t pageWrites = 0;  // statistics 
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
static const int FFIFO = 4;
static __thread int thread_id;
static __thread uint64_t startFrame;
static __thread uint64_t endFrame;
static __thread uint64_t count;


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
        uint64_t page = (addr-pt->virtmem) / PAGE_SIZE;
        //printf("\x1B[3%dm""Thread %d Faulting at %p and page %lu \n"RESET,thread_id+1,thread_id, addr, page);
        if(page<pt->npages) {
            pt->handler(pt, page);
            return;
        }
    }
    fprintf(stderr,"segmentation fault at address %p\n",addr);
    abort();
}

void readData(uint64_t page, char *data) {
    if (pagingSystem == MEM_PAGING)
        mem_read(mem, page, data);
    else if (pagingSystem == RMEM_PAGING)
        rmem_read(rmem, page, data);
    else if (pagingSystem == DISK_PAGING)
        disk_read(disk, page, data);
    else if (pagingSystem == RRMEM_PAGING)
         rrmem_read(rrmem, page, data);
    pageReads++;
}

void writeData(uint64_t page, char *data) {
    if (pagingSystem == MEM_PAGING)
        mem_write(mem, page, data);
    else if (pagingSystem == RMEM_PAGING)
        rmem_write(rmem, page, data);
    else if (pagingSystem == DISK_PAGING)
        disk_write(disk, page, data);
    else if (pagingSystem == RRMEM_PAGING)
        rrmem_write(rrmem, page, data);
    pageWrites++;
}

void LRU_page_fault_handler(struct page_table *pt, uint64_t page) {
    uint64_t frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    if(bits == 0) { // page is not in memory
        if(dllList->count < pt->nframes) {
            uint64_t i = dllList->count + startFrame;
            insertHashNode(page,createdllListNode(page));
            page_table_set_entry(pt, page, i, PROT_READ);
            readData(page,&pt->physmem[i*PAGE_SIZE]);
        }
        else {
            uint64_t tempPage = 0;
            deleteLRU(&tempPage);
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);

            readData(page,&pt->physmem[frame*PAGE_SIZE]);
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            insertHashNode(page,createdllListNode(page));
        }
    } else { // if page is already in table--need to set write bit
        moveListNodeToFront(getHashNode(page));
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
    }

    /*struct dllListNode *temp = dllList->head;
    for(uint64_t i = 0; i < dllList->count; i++)
    {
        printf("%" PRIu64 ", ",temp->key);
        temp = temp->next;
    }
    printf("\n\n");*/
}

void LFU_page_fault_handler(struct page_table *pt, uint64_t page) {
    uint64_t frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    if(bits == 0) { // page is not in memory
        if(freqList->count < pt->nframes) {
            uint64_t i = freqList->count + startFrame;
            insertHashNode(page,createlfuListNode(page));
            page_table_set_entry(pt, page, i, PROT_READ);
            readData(page,&pt->physmem[i*PAGE_SIZE]);
        }
        else {
            uint64_t tempPage = 0;
            deleteLFU(&tempPage);
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);

            readData(page,&pt->physmem[frame*PAGE_SIZE]);
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            insertHashNode(page,createlfuListNode(page));
        }
    } else { // if page is already in table--need to set write bit
        moveNodeToNextFreq(getHashNode(page));
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
    }

    /*struct freqListNode *fListnode = freqList->head;
    uint64_t i = 0;
    if (fListnode != NULL)
        while (fListnode != NULL)
        {
            printf("index: %" PRIu64 " | use cnt %" PRIu64 ":", i, fListnode->useCount);
            struct lfuListNode *lfuListNode = fListnode->head;
            while (lfuListNode != NULL)
            {
                printf("%" PRIu64 ", ", lfuListNode->key);
                lfuListNode = lfuListNode->next;
            }
            fListnode = fListnode->next;
            i++;
            printf("\n");
        }
    printf("\n\n");*/
}

void FIFO_page_fault_handler( struct page_table *pt, uint64_t page ) {
    uint64_t i;
    uint64_t frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    for(i = startFrame; i < endFrame; i++) { // increment values already in frameState--keeps track of oldest value in frame table
        if(frameState[i] != 0) {
            frameState[i]++;
        }
    }

    if(bits == 0) { // page is not in memory
        uint64_t emptyFrame = 0;
        for(i = startFrame; i < endFrame; i++) {
            // empty frame, we can insert a page
            if(frameState[i] == 0) {
                frameState[i] = 1;
                framePage[i] = page;
                emptyFrame = 1;
                page_table_set_entry(pt, page, i, PROT_READ);
                readData(page,&pt->physmem[i*PAGE_SIZE]);
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
            uint64_t tempPage = framePage[frame]; // get page currently in frame
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) { // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);
            }

            readData(page,&pt->physmem[frame*PAGE_SIZE]);
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

void RAND_page_fault_handler( struct page_table *pt, uint64_t page ) {
    uint64_t frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    if(bits == 0) { // page is not in memory
        if(count < pt->nframes) { //There is a free frame
            uint64_t i = count + startFrame;
            insertHashNode(page,NULL);
            page_table_set_entry(pt, page, i, PROT_READ);
            readData(page,&pt->physmem[i*PAGE_SIZE]);
            count++;
        }
        else {
            uint64_t randIndex = rand() % pt->nframes; // randomly remove a page
            while(hashTable[randIndex].head == NULL)
            {
                randIndex++;
                if(randIndex > pt->nframes-1)
                    randIndex = 0;
            }
            uint64_t tempPage = hashTable[randIndex].head->key;
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);

            readData(page,&pt->physmem[frame*PAGE_SIZE]);
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            deleteHashNode(tempPage);
            insertHashNode(page, NULL);
        }
    } else  // if page is already in table--need to set write bit
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
}

void FFIFO_page_fault_handler( struct page_table *pt, uint64_t page ) {
    uint64_t frame;
    int bits;

    pageFaults++;
    page_table_get_entry(pt, page, &frame, &bits); // check if the page is in memory
    if(bits == 0) { // page is not in memory
        if(dllList->count < pt->nframes) {
            uint64_t i = dllList->count + startFrame;
            insertHashNode(page,createdllListNode(page));
            page_table_set_entry(pt, page, i, PROT_READ);
            readData(page,&pt->physmem[i*PAGE_SIZE]);
        }
        else {
            uint64_t tempPage = dllList->tail->key;
            deleteHashNode(tempPage);
            deletedllListNode(dllList->tail);
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);

            readData(page,&pt->physmem[frame*PAGE_SIZE]);
            page_table_set_entry(pt, page, frame, PROT_READ);
            page_table_set_entry(pt, tempPage, frame, 0);
            insertHashNode(page,createdllListNode(page));
        }
    } else  // if page is already in table--need to set write bit
        page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
    
    /*struct dllListNode *temp = dllList->head;
    for(uint64_t i = 0; i < dllList->count; i++)
    {
        printf("%" PRIu64 ", ",temp->key);
        temp = temp->next;
    }
    printf("\n\n");*/
}

void sigterm(){
    printf("Intercepting SIGINT\n");
    struct page_table *pt = the_page_table;
    clean_page_table(pt);
}

struct page_table *init_virtual_memory(uint64_t npages, uint64_t nframes, const char* system, const char* algo) {
    struct page_table *pt;
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sigterm;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGINT, &action, NULL);
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

    if (!strcmp(algo, "ffifo"))
    	replacementPolicy = FFIFO;
    else if (!strcmp(algo, "fifo"))
        replacementPolicy = FIFO;
    else if (!strcmp(algo, "lru"))
    	replacementPolicy = LRU;
    else if (!strcmp(algo, "lfu"))
    	replacementPolicy = LFU;
    else if (!strcmp(algo, "rand"))
    	replacementPolicy = RAND;
    else {
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
    else if(pagingSystem == RRMEM_PAGING) {
        rrmem = rrmem_allocate(npages);
        if(!rrmem) {
            fprintf(stderr,"couldn't create virtual rrmem: %s\n",strerror(errno));
            abort();
        }
    }

    if(replacementPolicy == FIFO) {
        uint64_t i;
        frameState = calloc(nframes, sizeof(int)); // allocate space for array of frameStates
        for(i = 0; i < nframes; i++) 
            frameState[i] = 0;
        framePage = calloc(nframes, sizeof(uint64_t)); // allocate space for array of framePages
        for(i = 0; i < nframes; i++) 
            framePage[i] = 0;
        pt = page_table_create( npages, nframes, FIFO_page_fault_handler );
    }
    else if(replacementPolicy == LRU) {
    	hashTable = (struct hash *) calloc(nframes, sizeof(struct hash));
        dllList = (struct dllList*) calloc(nframes, sizeof(struct dllListNode));
        dllList->count = 0;
        pt = page_table_create( npages, nframes, LRU_page_fault_handler );
    }
    else if(replacementPolicy == LFU) {
        hashTable = (struct hash *) calloc(nframes, sizeof(struct hash));
        freqList = (struct freqList*) calloc(nframes, sizeof(struct lfuListNode));
        freqList->count = 0;
        pt = page_table_create( npages, nframes, LFU_page_fault_handler );
    }
    else if(replacementPolicy == RAND) {
        hashTable = (struct hash *) calloc(nframes, sizeof(struct hash));
        pt = page_table_create( npages, nframes, RAND_page_fault_handler );
    }
    else if(replacementPolicy == FFIFO) {
        hashTable = (struct hash *) calloc(nframes, sizeof(struct hash));
        dllList = (struct dllList*) calloc(nframes, sizeof(struct dllListNode));
        dllList->count = 0;
        pt = page_table_create( npages, nframes, FFIFO_page_fault_handler );
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
    rmem_init_thread_sockets(thread_id);
    printf("\x1B[3%dm""Thread %d Start Frame %" PRIu64 " End Frame %" PRIu64 " \n"RESET,t_id+1, t_id, startFrame, endFrame);
    if(replacementPolicy == LRU || replacementPolicy == FFIFO) {
        hashTable = (struct hash *) calloc(pt->nframes, sizeof(struct hash));
        dllList = (struct dllList*) calloc(pt->nframes, sizeof(struct dllListNode));
        dllList->count = 0;
    }
    else if(replacementPolicy == LFU) {
        hashTable = (struct hash *) calloc(pt->nframes, sizeof(struct hash));
        freqList = (struct freqList*) calloc(pt->nframes, sizeof(struct lfuListNode));
        freqList->count = 0;
    }
    else if(replacementPolicy == RAND) {
        hashTable = (struct hash *) calloc(pt->nframes, sizeof(struct hash));
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
    if (replacementPolicy == FIFO) {
    	frameState = realloc(frameState, pt->nframes*num_threads*sizeof(int));
    	if (!frameState) perror("realloc"), free(frameState), exit(0);
    	framePage = realloc(framePage, pt->nframes*num_threads*sizeof(uint64_t));
    	if (!framePage) perror("realloc"), free(framePage), exit(0);
    	memset(frameState, 0, pt->nframes*num_threads*sizeof(int));
    	memset(framePage, 0, pt->nframes*num_threads*sizeof(uint64_t));
    }
}

struct page_table *page_table_create(uint64_t npages, uint64_t nframes, page_fault_handler_t handler) {
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

    if (ftruncate(pt->fd, page_space) < 0)
        perror("ftruncate");

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
    pt->page_mapping = calloc(npages, sizeof(uint64_t));
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
    uint64_t frame;
    int bits;
    if(replacementPolicy == FIFO) {
        for(uint64_t i = 0; i < pt->nframes; i++) { 
            uint64_t tempPage = framePage[i]; // get page we want to kick out 
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE)) // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);

            page_table_set_entry(pt, tempPage, frame, PROT_NONE);
            frameState[frame] = 0;
        }
    }
    else if(replacementPolicy == LRU || replacementPolicy == FFIFO) {
        while(dllList->count > 0) {
            uint64_t tempPage;
            deleteLRU(&tempPage);
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE))  // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);
            
            page_table_set_entry(pt, tempPage, frame, PROT_NONE);
        }
    }
    else if(replacementPolicy == LFU) {
        while(freqList->count > 0) {
            uint64_t tempPage;
            deleteLFU(&tempPage);
            page_table_get_entry(pt, tempPage, &frame, &bits);
            if(bits == (PROT_READ|PROT_WRITE))  // if page has been written
                writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]);
            
            page_table_set_entry(pt, tempPage, frame, PROT_NONE);
        }
    }
    else if(replacementPolicy == RAND)
    {
        struct hashNode *currNode; 
        for(uint64_t i = 0; i < pt->nframes; i++) { 
            if (hashTable[i].count == 0) 
                continue; 
            currNode = hashTable[i].head; 
            if (!currNode) 
                continue; 
            while (currNode != NULL) { 
                uint64_t tempPage = currNode->key; 
                page_table_get_entry(pt, tempPage, &frame, &bits); 
                if(bits == (PROT_READ|PROT_WRITE))  // if page has been written 
                    writeData(tempPage,&pt->physmem[frame*PAGE_SIZE]); 
                page_table_set_entry(pt, tempPage, frame, PROT_NONE);
                deleteHashNode(tempPage); 
                currNode = hashTable[i].head;
            }
        } 
    }
}

void close_thread_sockets() {
    rmem_close_thread_sockets();
}

void page_table_delete(struct page_table *pt) {
    rmem_close_thread_sockets();
    uint64_t page_space = PAGE_SIZE * (uint64_t) pt->npages;
    uint64_t frame_space = PAGE_SIZE * (uint64_t) pt->nframes;
    munmap(pt->virtmem,page_space);
    munmap(pt->physmem,frame_space);
    free(pt->page_bits);
    free(pt->page_mapping);
    close(pt->fd);
    free(pt);
}

void page_table_set_entry(struct page_table *pt, uint64_t page, uint64_t frame, int bits) {
    if(page>=pt->npages) {
        fprintf(stderr,"page_table_set_entry: illegal page #%" PRIu64 "\n",page);
        abort();
    }

    if(frame<startFrame || frame>=endFrame) {
        fprintf(stderr,"page_table_set_entry: illegal frame #%" PRIu64 "\n",frame);
        abort();
    }
    pt->page_mapping[page] = frame;
    pt->page_bits[page] = bits;

    remap_file_pages(pt->virtmem+page*PAGE_SIZE, PAGE_SIZE, 0, frame, 0);
    mprotect(pt->virtmem+page*PAGE_SIZE, PAGE_SIZE, bits);
}

void page_table_get_entry(struct page_table *pt, uint64_t page, uint64_t *frame, int *bits) {
    if(page>=pt->npages) {
        fprintf(stderr,"page_table_get_entry: illegal page #%" PRIu64 "\n",page);
        abort();
    }
    *frame = pt->page_mapping[page];
    *bits = pt->page_bits[page];
}

void page_table_print_entry(struct page_table *pt, uint64_t page) {
    if(page>=pt->npages) {
        fprintf(stderr,"page_table_print_entry: illegal page #%" PRIu64 "\n",page);
        abort();
    }

    int b = pt->page_bits[page];
    printf("page %06" PRIu64 ": frame %06" PRIu64 " bits %c%c%c, vaddr %p\n",
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
    uint64_t i;
    for(i=0;i<pt->npages;i++) {
        page_table_print_entry(pt,i);
    }
}
void frame_table_print_entry(struct page_table *pt, uint64_t frame) {
    printf("frame %06" PRIu64 ": page %06" PRIu64 " state %d, vaddr %p\n",
        frame,
        framePage[frame],
        frameState[frame],
        &pt->physmem[frame*PAGE_SIZE]
   );
}

void frame_table_print(int num_threads) {
    if (!num_threads) num_threads =1;
    struct page_table *pt = the_page_table;
    uint64_t i;
    for(i=0;i<pt->nframes*num_threads;i++) {
        frame_table_print_entry(pt, i);
    }
}

uint64_t page_table_get_nframes(struct page_table *pt) {
    return pt->nframes;
}

uint64_t page_table_get_npages(struct page_table *pt) {
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
    printf("page faults: %" PRIu64 "     reads: %" PRIu64 "     writes: %" PRIu64 "\n", pageFaults, pageReads, pageWrites);
}

void clean_page_table(struct page_table *pt) {
    if(replacementPolicy == LRU || replacementPolicy == FFIFO) {
        while(dllList->count > 0) {
            uint64_t tempPage = 0;
            deleteLRU(&tempPage);
        }
    }
    else if(replacementPolicy == LFU) {
        while(freqList->count > 0) {
            uint64_t tempPage = 0;
            deleteLFU(&tempPage);
        }
    }
    else if(replacementPolicy == RAND)
    {
        struct hashNode *currNode; 
        for(uint64_t i = 0; i < pt->nframes; i++) { 
            if (hashTable[i].count == 0) 
                continue; 
            currNode = hashTable[i].head; 
            if (!currNode) 
                continue; 
            while (currNode != NULL) { 
                uint64_t tempPage = currNode->key; 
                deleteHashNode(tempPage); 
                currNode = hashTable[i].head;
            }
        } 
    }

    free(hashTable);
    free(dllList);
    free(freqList);
    free(framePage);
    free(frameState);
    if (pagingSystem == DISK_PAGING)
        disk_close(disk);
    else if (pagingSystem == RMEM_PAGING)
        rmem_deallocate(rmem);
    else if (pagingSystem == RRMEM_PAGING)
        rrmem_deallocate(rrmem);
    pageFaults = 0;
    pageReads = 0;
    pageWrites = 0;
    page_table_delete(pt);
}


struct dllListNode *createdllListNode(uint64_t key) {
    struct dllListNode *newnode;
    newnode = (struct dllListNode *) malloc(sizeof(struct dllListNode));
    newnode->key = key;
    newnode->prev = NULL;
    newnode->next = dllList->head;
    if (dllList->count > 0)
        dllList->head->prev = newnode;
    else
        dllList->tail = newnode;
    dllList->head = newnode;
    dllList->count++;
    return newnode;
}

struct lfuListNode *createlfuListNode(uint64_t key) {
    struct lfuListNode *newnode;
    newnode = (struct lfuListNode *) malloc(sizeof(struct lfuListNode));
    newnode->key = key;
    newnode->next = NULL;
    newnode->prev = NULL;
    if (freqList->head != NULL && freqList->head->useCount == 0) {
        newnode->next = freqList->head->head;
        freqList->head->head->prev = newnode;
        freqList->head->head = newnode;
    }
    else {
        struct freqListNode *newFreqNode;
        newFreqNode = (struct freqListNode *) malloc(sizeof(struct freqListNode));
        newFreqNode->useCount = 0;
        newFreqNode->head = newnode;
        newFreqNode->tail = newnode;
        newFreqNode->next = NULL;
        newFreqNode->prev = NULL;
        if (freqList->head != NULL) {
            freqList->head->prev = newFreqNode;
            newFreqNode->next = freqList->head;
        }
        freqList->head = newFreqNode;
    }
    newnode->parent = freqList->head;
    freqList->count++;
    return newnode;
}

void moveListNodeToFront(struct dllListNode *node) {
    if(dllList->head == node)
        return;
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    node->next = dllList->head;
    if(dllList-> tail == node)
        dllList->tail = node->prev;
    node->prev = NULL;
    if (dllList->count > 1)
        dllList->head->prev = node;
    else {
        dllList->head = node;
        dllList->tail = node;
    }
    dllList->head = node;
}

void moveNodeToNextFreq(struct lfuListNode *node) {
    if (node->parent->next == NULL) { //Check if the next freq exists, if not make it
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
    else if (node->parent->next->useCount > node->parent->useCount + 1) { //check if next frequency is +1
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

    if (temp->head == NULL) {
        if (temp == freqList->head)
            freqList->head = temp->next;
        if (temp->prev != NULL)
            temp->prev->next = temp->next;
        if (temp->next != NULL)
            temp->next->prev = temp->prev;
        free(temp);
    }
}


void deletedllListNode(struct dllListNode *node) {
    if (node == dllList->head)
        dllList->head = node->next;
    if (node == dllList->tail)
        dllList->tail = node->prev;
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    dllList->count--;
    free(node);
}

void deletelfuListNode(struct lfuListNode *node) {
    if (node == node->parent->head)
        node->parent->head = node->next;
    if (node == node->parent->tail)
        node->parent->tail = node->prev;
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    if (node->parent->head == NULL) {
        if (node->parent == freqList->head)
            freqList->head = node->parent->next;
        node->parent->prev = node->parent->next;
        node->parent->next = node->parent->prev;
    }
    free(node);
    freqList->count--;
}

struct hashNode * createHashNode(uint64_t key) {
    struct hashNode *newnode;
    newnode = (struct hashNode *) malloc(sizeof(struct hashNode));
    newnode->key = key;
    newnode->next = NULL;
    return newnode;
}

uint64_t hashFunction(uint64_t key) {
    return key % the_page_table->nframes;
}

void insertHashNode(uint64_t key, void* listnodePointer) {
    uint64_t hashIndex = hashFunction(key);
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

void deleteHashNode(uint64_t key) {
    // find the bucket using hash index
    uint64_t hashIndex = hashFunction(key);
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

void* getHashNode(uint64_t key) {
    uint64_t hashIndex = hashFunction(key);
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

void deleteLRU(uint64_t *page) {
    if (dllList->tail != NULL) {
        *page = dllList->tail->key;
        deleteHashNode(dllList->tail->key);
        deletedllListNode(dllList->tail);
    }
}

void deleteLFU(uint64_t *page) {
    if (freqList->head != NULL) {
        *page = freqList->head->tail->key;
        deleteHashNode(freqList->head->tail->key);
        deletelfuListNode(freqList->head->tail);
    }
}
