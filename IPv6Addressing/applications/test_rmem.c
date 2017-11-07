#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include "../lib/vmem/page_table.h"
#include "../lib/vmem/rmem.h"

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define RESET "\033[0m"

static inline uint64_t getns(void)

{
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(ret == 0);
    return (((uint64_t)ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

static int compare_bytes( const void *pa, const void *pb ) {
    int a = *(char*)pa;
    int b = *(char*)pb;

    if(a<b) {
        return -1;
    } else if(a==b) {
        return 0;
    } else {
        return 1;
    }

}

void focus_program( char *data, int length ) {
    int total=0;
    int i,j;

    srand(38290);

    for(i=0;i<length;i++) {
        data[i] = 0;
    }

    for(j=0;j<100;j++) {
        int start = rand()%length;
        int size = 25;
        for(i=0;i<100;i++) {
            data[ (start+rand()%size)%length ] = rand();
        }
    }

    for(i=0;i<length;i++) {
        total += data[i];
    }

    printf("focus result is %d\n",total);
}

void sort_program( char *data, int length) {
    int total = 0;
    int i;

    srand(4856);
    printf("Writing values\n");;
    uint64_t rStart = getns();
    for(i=0;i<length;i++) {
        data[i] = rand();
    }

    uint64_t latency_write = getns() - rStart;
    printf("Sorting values\n");
    rStart = getns();
    qsort(data,length,1,compare_bytes);
    uint64_t latency_sort = getns() - rStart;

    printf("Reading values\n");
    rStart = getns();
    for(i=0;i<length;i++) {
        total += data[i];
    }
    uint64_t latency_read = getns() - rStart;

    printf("sort result is %d\n",total);
    printf("Write time...: %lu micro seconds\n", latency_write/1000);
    printf("Sort time...: %lu micro seconds\n", latency_sort/1000);
    printf("Read time...: %lu micro seconds\n", latency_read/1000);

}

void scan_program( char *cdata, int length) {
    unsigned i, j;
    unsigned char *data = (unsigned char *) cdata;
    unsigned total = 0;

    printf("Writing values\n");;
    uint64_t rStart = getns();
    for(i=0;i<length;i++) {
        data[i] = i%256;
    }
    uint64_t latency_write = getns() - rStart;

    printf("Summing up values\n");
    rStart = getns();
    for(j=0;j<10;j++) {
        for(i=0;i<length;i++) {
            total += data[i];
        }
    }
    uint64_t latency_read = getns() - rStart;

    printf("scan result is %u\n",total);
    printf("Write time...: "KGRN"%lu"RESET" micro seconds\n", latency_write/1000);
    printf("Sum time...: "KGRN"%lu"RESET" micro seconds\n", latency_read/1000);
    printf("Total time taken: "KGRN"%lu"RESET" micro seconds\n", (latency_read + latency_write)/1000 );
}

void simple_test( char *cdata, int length) {
    unsigned char *data = (unsigned char *) cdata;
    uint64_t totalR = 0;
    uint64_t totalW = 0;

    printf("%d iterations\n", length);
    for (int i = 0; i< length; i++) {
        uint64_t rStart = getns();
        data[i] = i;
        totalR += getns() - rStart;
    }
    for (int i = 0; i< length; i++) {
        uint64_t wStart = getns();
        data[i] = data[i]+1;
        totalW += getns() - wStart;
    }
    printf("Read time total...: %lu micro seconds\n", totalR/1000);
    printf("Read time average...: %lu nano seconds\n", totalR/length);
    printf("Write time total...: %lu micro seconds\n", totalW/1000);
    printf("Write time average...: %lu nano seconds\n", totalW/length);
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>

int isWord(char prev, char cur) {
    return isspace(cur) && isgraph(prev);
}

/* create thread argument struct for thr_func() */
typedef struct _thread_data_t {
  int tid;
  char *data;
  int length;
  int count;
} thread_data_t;


void *wc(void *arg) {
    char prev = ' ';
    thread_data_t *data = (thread_data_t *)arg;
    char *cdata = data->data;
    printf("Hello!\n");
    printf("%d\n", data->length);
    for (int index = 0; index < data->length; index++) {
        // TODO: Should also handle \n (i.e. any whitespace)
        //       should also ensure that it's only a word if
        //       there was a non white space character before it.
        if (isWord(prev, cdata[index])) {
            data->count++;
        }
        prev = cdata[index];
    }

}

#define NUM_THREADS 1
void wc_program_threads(char *cdata, int length) {
    pthread_t thr[NUM_THREADS];
    int rc;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];
    char symbol;
    int i =0;
    FILE *fp = fopen("baskerville.txt", "rb");

    printf("Storing thingy\n");
    uint64_t rStart = getns();
    if(fp != NULL) {
        while((symbol = getc(fp)) != EOF) {
        cdata[i] = symbol; 
        i++;
        }
        fclose(fp);
    }
    int fileLenght = i;
    uint64_t latency_store = getns() - rStart;
    printf("Done with storing thingy\n");
     
      /* create threads */
    for (i = 0; i < NUM_THREADS; i++) {
        int split = fileLenght/NUM_THREADS;
        thr_data[i].tid = i;
        thr_data[i].data = &cdata[split * i];
        if (i == NUM_THREADS-1)
            thr_data[i].length = length - split * i;
        else
            thr_data[i].length = split;
        printf("%d %d %d\n", length, thr_data[i].length, split );
        if ((rc = pthread_create(&thr[i], NULL, wc, &thr_data[i]))) {
          fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        }
    }
    /* block until all threads complete */
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(thr[i], NULL);
    }
    printf("Reading from thingy..\n");
    rStart = getns();

    int count = 0;
    for (i = 0; i < NUM_THREADS; ++i) {
        count += thr_data[i].count;
    }
    printf("Word count: %d\n", count);
    uint64_t latency_read = getns() - rStart;
    printf("Storing time...: "KGRN"%lu"RESET" micro seconds\n", latency_store/1000);
    printf("Reading time...: "KGRN"%lu"RESET" micro seconds\n", latency_read/1000);
    printf("Total time taken: "KGRN"%lu"RESET" micro seconds\n", (latency_read + latency_store)/1000 );
}

void wc_program(char *cdata, int length) {
    FILE *fp = fopen("baskerville.txt", "rb");

    printf("Storing thingy\n");
    char symbol;
    int i =0;
    uint64_t rStart = getns();
    if(fp != NULL) {
        while((symbol = getc(fp)) != EOF) {
        cdata[i] = symbol; 
        i++;
        }
        fclose(fp);
    }
    uint64_t latency_store = getns() - rStart;
    printf("Done with storing thingy\n");
    int count = 0, index;
    char prev = ' ';

    printf("Reading from thingy..\n");
    rStart = getns();
    for (index = 0; index < length; index++) {
        // TODO: Should also handle \n (i.e. any whitespace)
        //       should also ensure that it's only a word if
        //       there was a non white space character before it.
        if (isWord(prev, cdata[index])) {
            count++;
        }
        prev = cdata[index];
    }
    uint64_t latency_read = getns() - rStart;
    printf("Word count: %d\n", count);
    printf("Storing time...: "KGRN"%lu"RESET" micro seconds\n", latency_store/1000);
    printf("Reading time...: "KGRN"%lu"RESET" micro seconds\n", latency_read/1000);
    printf("Total time taken: "KGRN"%lu"RESET" micro seconds\n", (latency_read + latency_store)/1000 );
}
int main( int argc, char *argv[] )
{
    if(argc!=5) {
        printf("use: virtmem <npages> <nframes> <disk|rmem|mem> <sort|scan|focus|test|grep>\n");
        return EXIT_FAILURE;
    }

    int npages = atoi(argv[1]);
    int nframes = atoi(argv[2]);
    const char *program = argv[4];
    const char *algo = argv[3]; // store algorithm command line argument

    sync();
    system("echo 3 > /proc/sys/vm/drop_caches");
    if(!strcmp(algo,"rmem"))
        set_vmem_config("tmp/config/distMem.cnf");
    struct page_table *pt = init_virtual_memory(npages, nframes, algo);
    char *virtmem = page_table_get_virtmem(pt);


    printf("Running "KRED"%s"RESET" benchmark...\n", algo);
    if(!strcmp(program,"sort")) {
        sort_program(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"scan")) {
        scan_program(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"focus")) {
        focus_program(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"test")) {
        simple_test(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"wc")) {
        wc_program(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"wc_t")) {
        wc_program(virtmem,npages*PAGE_SIZE);
    } else {
        fprintf(stderr,"unknown program: %s\n",argv[4]);
    }
    if (strcmp(algo,"mem")) {
        print_page_faults();
        clean_page_table(pt);
    }
    return 0;
}
