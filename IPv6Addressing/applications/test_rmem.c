#define _GNU_SOURCE
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include "../lib/vmem/page_table.h"
#include "../lib/vmem/rmem.h"
#include "../lib/client_lib.h"
#include "../lib/utils.h"

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define RESET "\033[0m"

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
struct ThreadData {
    int id;
    int start, stop;
    int* array;
};
#define NUM_THREADS 8

/* create thread argument struct for thr_func() */
typedef struct _thread_data_t {
  int tid;
  char *data;
  int length;
  int count;
} thread_data_t;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


int isWord(char prev, char cur) {
    return isspace(cur) && isgraph(prev);
}

void *wc(void *arg) {
    printf("Reading from thingy..\n");
    char prev = ' ';
    thread_data_t *data = (thread_data_t *)arg;

    /* Set affinity mask to include CPUs 0 to 7 */
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    int assigned = data->tid % num_cores;
    pthread_t my_thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(assigned, &cpuset);
    pthread_setaffinity_np(my_thread, sizeof(cpu_set_t), &cpuset);
    printf("Assigned Thread %d to core %d\n", data->tid, assigned );

    set_thread_id_tx(data->tid);
    set_thread_id_rx(data->tid);
    struct config myConf = get_bb_config();
    struct sockaddr_in6 *temp = init_sockets(&myConf);
    init_thread(data->tid);
    char *cdata = data->data;
    for (int i = 0; i < data->length; i++) {
            // TODO: Should also handle \n (i.e. any whitespace)
            //       should also ensure that it's only a word if
            //       there was a non white space character before it.
            if (isspace(cdata[i]) && isgraph(prev)) {
                data->count++;
            }
            //printf("%c",cdata[index] );
            prev = cdata[i];
    }
/*    pthread_mutex_lock(&mutex);
    printf("******Thread %d done with Counting******\n", data->tid);
    page_table_print();
    printf("******Thread %d Counting Result: %d******\n", data->tid, data->count);
    pthread_mutex_unlock(&mutex);*/

    return NULL;
}

void wc_program_threads(char *cdata, int length) {
    pthread_t thr[NUM_THREADS];
    int rc;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];
    int i =0;
    FILE *fp = fopen("baskerville.txt", "rb");
    printf("Reading in thingy\n");
    uint64_t rStart = getns();
    if(fp != NULL) {
        char symbol;
        while((symbol = getc(fp)) != EOF) {
            cdata[i] = symbol; 
            i++;
        }
        fclose(fp);
    }
    int fileLenght = i;
    uint64_t latency_store = getns() - rStart;
    printf("Done with reading thingy\n");
    int split = fileLenght/NUM_THREADS + (BLOCK_SIZE -((fileLenght/NUM_THREADS) % BLOCK_SIZE));

    printf("******Done with storing******\n");
    page_table_flush();
    printf("******Done with cleaning******\n");
    register_threads(NUM_THREADS);
    for (i = 0; i < NUM_THREADS; i++) {
        thr_data[i].tid = i;
        thr_data[i].count = 0;
        int offset = split * i;
        thr_data[i].data = cdata + offset;
        if (i == NUM_THREADS-1)
            thr_data[i].length = fileLenght - offset;
        else
            thr_data[i].length = split;
        printf("Launching Thread %d\n",i );
        printf("Total length %.3f  Thread length %.3f Ideal split %.3f\n", (double)fileLenght/BLOCK_SIZE, (double)thr_data[i].length/BLOCK_SIZE, (double)split/BLOCK_SIZE );
        if ((rc = pthread_create(&thr[i], NULL, wc, &thr_data[i]))) {
          fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        }
    }
    /* block until all threads complete */
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(thr[i], NULL);
    }
    int count = 0;
    for (i = 0; i < NUM_THREADS; ++i) {
        count += thr_data[i].count;
    }
    printf("Word count: %d\n", count);
    printf("Storing time...: "KGRN"%lu"RESET" micro seconds\n", latency_store/1000);
    //printf("Reading time...: "KGRN"%lu"RESET" micro seconds\n", latency_read/1000);
    int latency_read = 0;
    printf("Total time taken: "KGRN"%lu"RESET" micro seconds\n", (latency_read + latency_store)/1000 );
}

void wc_program(char *cdata, int length) {

    FILE *fp = fopen("baskerville.txt", "rb");
    printf("Reading in thingy\n");
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
    int fileLenght = i;
    uint64_t latency_store = getns() - rStart;
    printf("Done with reading thingy\n");
    page_table_flush();
    int count = 0, index;
    char prev = ' ';

    printf("Reading from thingy..\n");
    rStart = getns();
    for (index = 0; index < fileLenght; index++) {
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
        printf("use: virtmem <npages> <nframes> <disk|rmem|mem> <sort|scan|focus|test|wc|wc_t>\n");
        return EXIT_FAILURE;
    }

    uint64_t npages = atoi(argv[1]);
    uint64_t nframes = atoi(argv[2]);
    printf("Pages %lu\n",npages );
    const char *program = argv[4];
    const char *algo = argv[3]; // store algorithm command line argument

    //sync();
    //system("echo 3 > /proc/sys/vm/drop_caches");
    set_vmem_config("tmp/config/distMem.cnf");
    //set_vmem_config("distmem_client.cnf");
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
        wc_program_threads(virtmem,npages*PAGE_SIZE);
    } else {
        fprintf(stderr,"unknown program: %s\n",argv[4]);
    }
    if (strcmp(algo,"mem")) {
        print_page_faults();
        clean_page_table(pt);
    }
    return 0;
}
