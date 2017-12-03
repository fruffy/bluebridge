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

#define NUM_THREADS 8

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

/* create thread argument struct for thr_func() */
typedef struct _thread_data_t {
  int tid;
  char *data;
  int length;
  int count;
} thread_data_t;


int isWord(char prev, char cur) {
    return isspace(cur) && isgraph(prev);
}
// Wiki 1000227250  6508938585 64552443006
void *wc(void *arg) {
    char prev = ' ';
    thread_data_t *data = (thread_data_t *)arg;

    // Assign threads to cores
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    int assigned = data->tid % num_cores;
    pthread_t my_thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(assigned, &cpuset);
    pthread_setaffinity_np(my_thread, sizeof(cpu_set_t), &cpuset);
    printf("Assigned Thread %d to core %d\n", data->tid, assigned );
    
    // BlueBridge initialization calls for threads
    struct config myConf = get_bb_config();
    struct sockaddr_in6 *temp = init_net_thread(data->tid, &myConf, 0);
    init_vmem_thread(data->tid);
    
    // Do the actual computation
    char *cdata = data->data;
    for (int i = 0; i < data->length; i++) {
            // TODO: Should also handle \n (i.e. any whitespace)
            //       should also ensure that it's only a word if
            //       there was a non white space character before it.
            if (isspace(cdata[i]) && isgraph(prev)) {
                data->count++;
            }
            prev = cdata[i];
    }
    return NULL;
}

void wc_program_threads(char *cdata, int npages, int nframes, const char *input) {
    pthread_t thr[NUM_THREADS];
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];
    uint64_t i = 0;
    FILE *fp = fopen(input, "rb");
    printf("Reading in text file\n");
    uint64_t rStart = getns();
    if(fp != NULL) {
        char symbol;
        while((symbol = getc(fp)) != EOF) {
            cdata[i] = symbol; 
            i++;
        }
        fclose(fp);
    }
    uint64_t fileLenght = i;
    uint64_t latency_store = getns() - rStart;

    printf("******Done with storing******\n");
    uint64_t split = fileLenght/NUM_THREADS + (BLOCK_SIZE -((fileLenght/NUM_THREADS) % BLOCK_SIZE));
    // Split the virtual memory table to give each thread its own cache
    register_vmem_threads(NUM_THREADS);
    pthread_attr_t attr;
    uint64_t  stacksize = 0;

/*    pthread_attr_init( &attr );
    pthread_attr_getstacksize( &attr, &stacksize );
    printf("before stacksize : [%lu]\n", stacksize);
    pthread_attr_setstacksize( &attr, stacksize + (uint64_t) nframes/NUM_THREADS * PAGE_SIZE );
    pthread_attr_getstacksize( &attr, &stacksize );
    printf("after  stacksize : [%lu]\n", stacksize);
    */
    pthread_attr_init( &attr );
    pthread_attr_getstacksize( &attr, &stacksize );
    pthread_attr_setstacksize( &attr, 99800000 );
    pthread_attr_getstacksize( &attr, &stacksize );
    // Split the dataset (more or less works) 
    for (i = 0; i < NUM_THREADS; i++) {
        thr_data[i].tid = i;
        thr_data[i].count = 0;
        uint64_t offset = split * i;
        thr_data[i].data = cdata + offset;
        if (i == NUM_THREADS-1)
            thr_data[i].length = fileLenght - offset;
        else
            thr_data[i].length = split;
        printf("Launching Thread %lu\n", i );
        printf("Total length %.3f  Thread length %.3f Ideal split %.3f\n", (double)fileLenght/BLOCK_SIZE, (double)thr_data[i].length/BLOCK_SIZE, (double)split/BLOCK_SIZE );
        if ((pthread_create(&thr[i], NULL, wc, &thr_data[i])) < 0) {
          perror("error: pthread_create");
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
    uint64_t latency_read = 0;
    printf("Total time taken: "KGRN"%lu"RESET" micro seconds\n", (latency_read + latency_store)/1000 );
}

void wc_program(char *cdata, int npages, int nframes, const char *input) {

    // Reading in the file
    uint64_t i =0;
    FILE *fp = fopen(input, "rb");
    printf("Reading in text file\n");
    uint64_t rStart = getns();
    if(fp != NULL) {
        char symbol;
        while((symbol = getc(fp)) != EOF) {
            cdata[i] = symbol; 
            i++;
        }
        fclose(fp);
    }
    uint64_t fsize = i;
    uint64_t latency_store = getns() - rStart;
    char prev = ' ';
    uint64_t count = 0;
    printf("Reading from thingy..\n");
    rStart = getns();
    for (uint64_t index = 0; index < fsize; index++) {
        // TODO: Should also handle \n (i.e. any whitespace)
        //       should also ensure that it's only a word if
        //       there was a non white space character before it.
        if (isWord(prev, cdata[index])) {
            count++;
        }
        prev = cdata[index];
    }
    uint64_t latency_read = getns() - rStart;
    printf("Word count: %lu\n", count);
    printf("Storing time...: "KGRN"%lu"RESET" micro seconds\n", latency_store/1000);
    printf("Reading time...: "KGRN"%lu"RESET" micro seconds\n", latency_read/1000);
    printf("Total time taken: "KGRN"%lu"RESET" micro seconds\n", (latency_read + latency_store)/1000 );
}

//pagerank structures
typedef struct {
    int *array;
    size_t used;
    size_t size;
} IntArr;

typedef struct vertex {
    double incoming_rank;   // Incoming rank for this vertex
    int num_edges;      // Number of edges
    int edge_offset;    // Offset into the edges array
} vertex;

double* edges;      // An array of all outgoing edges.
double* edgenorm;   // number of outgoing edges for vertex j
vertex* vertices;   // An array of all vertices in the graph
double* rank;       // Page rank value for vertex j
IntArr  ids;        // List of array ids (dynamically growing)
int     num_vertices;   // Number of vertices in graph\

// Dynamic array functions
void initArray(IntArr *a, size_t initial_size) {
    a->array = (int*) malloc(initial_size*sizeof(int));
    a->used = 0;
    a->size = initial_size;
}

void insertArray(IntArr *a, int element) {
    if (a->used == a->size) {
        a->size *=2;
        a->array = (int*) realloc(a->array, a->size*sizeof(int));
    }

    a->array[a->used++] = element;
}

void freeArray(IntArr *a) {
    free(a->array);
    a->array = NULL;
    a->used = a->size = 0;
}


void init_pr(char *cdata, int length, const char* input) {
    // Reading in the file
    printf("First read (get max and count)\n");
    uint64_t i =0;
    FILE *fp = fopen(input, "rb");
    //FILE *fp = fopen("web-Google.txt", "rb");
    char line[256];
    int v1, v2;
    int max = 0, count = 0;

    printf("starting measure loop");
    assert(fp != NULL);
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%d\t%d\n", &v1, &v2);
        if (v1 > max) {
            max = v1;
        }

        if (v2 > max) {
            max = v2;
        }
        count++;
    }
    printf("Edges: %d Verticies:%d\n",count,max);

    printf(("Rewinding file\n"));
    rewind(fp);

    printf("Num vertices: %d, Num edges: %d\n", max+1, count);
    num_vertices = (max+1);

    int offset = 0;
    printf("sizeof char %lu\n",sizeof(char));

    printf("Mallocing vs array.\n");

    IntArr* vs = (IntArr*) malloc(num_vertices*sizeof(IntArr));
    
    for (int i = 0; i < num_vertices; i++) {
        initArray(&(vs[i]), 100);
    }

    printf("Mallocing vertices array.\n");
    vertices = (vertex*) &(cdata[offset]);
    offset += num_vertices*sizeof(vertex);
    printf("Mallocing edgenorm array.\n");
    edgenorm = (double*) &(cdata[offset]);
    offset += num_vertices*sizeof(double);
    printf("Mallocing rank array.\n");
    rank = (double*) &(cdata[offset]);
    offset += num_vertices*sizeof(double);
    printf("Mallocing edges array.\n");
    edges = (double*) &(cdata[offset]);
    offset += count*sizeof(double);

    //printf(("Second pass of file.\n");
    printf("Second pass of file.\n");

    int counter = 0;
    printf("\n");
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%d\t%d\n", &v1, &v2);
        vertices[v1].num_edges++;
        insertArray(&vs[v1], v2);
        counter++;
        if (counter % 10000 == 0) {
            printf("\r%d/%d scanned\t",counter,count);
        }
    }

    printf("closing file.\n");
    fclose(fp);

    printf("Setting rest of variables.\n");
    int k = 0; 
    for (int i = 0; i < num_vertices; i++) {
        printf("Setting vertex: %d\n", i);
        rank[i] = 1;
        edgenorm[i] = vertices[i].num_edges + 1;
        vertices[i].edge_offset = k;

        for (int j = 0; j < vs[i].used; j++) {
            printf("j: %d, k: %d, i: %d, num_vertices: %d, count: %d\n", j, k,
                i, num_vertices, count);
            edges[k] = vs[i].array[j];
            k++;
        }
    }

    printf("Freeing dynamic array.\n");
    for (int i = 0; i < num_vertices; i++) {
        freeArray(&vs[i]);
    }

    free(vs);


    //Actually do some page rank
}

void pagerank(int rounds, double d) {
    double outrank = 0;
    double alpha = ((double) (1 - d))/((double) num_vertices);

    for (int i = 0; i < rounds; i++) {
        for (int j = 0; j < num_vertices; j++) {
            printf("Round: %d, Vertex: %d\n", i, j);
            outrank = rank[j]/edgenorm[j];
            printf("Outrank: %f, num_edges: %d\n", outrank, vertices[j].num_edges);
            for (int k = 0; k < vertices[j].num_edges; k++) {
                // TODO: check values
                int edge_index = vertices[j].edge_offset + k;
                //printf("Edge_offset: %d, k: %d, Edge index: %d\n", vertices[j].edge_offset, k, edge_index);
                int edge_to = edges[edge_index];
                //printf("Edge to: %d\n", edge_to);
                vertex* to_vertex = &vertices[edge_to];
                //printf("to vertex: %p\n", to_vertex);
                to_vertex->incoming_rank += outrank;
                //printf("to_vertex ir: %f, array ir: %f\n", to_vertex->incoming_rank,
                //  vertices[edge_to].incoming_rank);
            }
        }

        for (int j = 0; j < num_vertices; j++) {
            rank[j] = alpha + (d*vertices[j].incoming_rank);
            printf("Updating rank for %d to %f\n", j, rank[j]);
        }
    }
}

void pr_program(char *cdata, int length, const char *input) {
    init_pr(cdata, length, input);
    int rounds = 20;
    double damp = 0.8;
    pagerank(rounds, damp);
}




int main( int argc, char *argv[] )
{
    if(argc!=8) {
        printf("use: rmem_test config <npages> <nframes> <disk|rmem|rrmem|mem> <fifo|lru|lfu|rand> <sort|scan|focus|test|wc|wc_t>\n");
        return EXIT_FAILURE;
    }
    set_vmem_config(argv[1]);
    uint64_t npages = atoi(argv[2]);
    uint64_t nframes = atoi(argv[3]);
    printf("Pages %lu\n",npages );
    const char *system = argv[4]; 
    const char *algo = argv[5]; 
    const char *program = argv[6];
    const char *input = argv[7];

    //sync();
    //system("echo 3 > /proc/sys/vm/drop_caches");
    //set_vmem_config("tmp/config/distMem.cnf");
    //set_vmem_config("distmem_client.cnf");
    struct page_table *pt = init_virtual_memory(npages, nframes, system, algo);
    char *virtmem = pt->virtmem;

    printf("Running "KRED"%s"RESET" benchmark...\n", system);
    if(!strcmp(program,"sort")) {
        sort_program(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"scan")) {
        scan_program(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"focus")) {
        focus_program(virtmem,npages*PAGE_SIZE);
    } else if(!strcmp(program,"wc")) {
        wc_program(virtmem, npages, nframes, input);
    } else if(!strcmp(program,"wc_t")) {
        wc_program_threads(virtmem, npages, nframes, input);
    } else if(!strcmp(program,"pr")) {
        pr_program(virtmem,npages*PAGE_SIZE, input);
    } else {
        fprintf(stderr,"unknown program: %s\n",program);
    }
    if (strcmp(algo,"mem")) {
        print_page_faults();
        clean_page_table(pt);
    }
    return 0;
}
