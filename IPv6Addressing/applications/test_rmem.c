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

static int compare_bytes( const void *pa, const void *pb )
{
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

void focus_program( char *data, int length )
{
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

void sort_program( char *data, int length, struct page_table *pt)
{
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

void scan_program( char *cdata, int length, struct page_table *pt )
{
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

void simple_test( char *cdata, int length, struct page_table *pt )
{
	unsigned char *data = (unsigned char *) cdata;
	uint64_t total = 0;
	printf("%d iterations\n", length);
	for (int i = 0; i< length; i++) {
		uint64_t rStart = getns();
		data[i] = i;
		total += getns() - rStart;
	}
	printf("Write time total...: %lu micro seconds\n", total/1000);
	printf("Write time average...: %lu nano seconds\n", total/length);
}


int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <disk|rmem> <sort|scan|focus>\n");
		return EXIT_FAILURE;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	const char *program = argv[4];
	const char *algo = argv[3]; // store algorithm command line argument

	sync();
	system("echo 3 > /proc/sys/vm/drop_caches");
	struct page_table *pt;
	char *virtmem;
	
	if (!strcmp(algo,"mem")) {
		pt = 0;
		virtmem = malloc(npages*PAGE_SIZE);
	} else {
		pt = init_virtual_memory(npages, nframes, algo);
		virtmem = page_table_get_virtmem(pt);
	}
	printf("Running "KRED"%s"RESET" benchmark...\n", algo);
	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE, pt);
	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE, pt);
	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);
	} else if(!strcmp(program,"test")) {
		simple_test(virtmem,npages*PAGE_SIZE, pt);
	} else {
		fprintf(stderr,"unknown program: %s\n",argv[4]);
	}

	if (strcmp(algo,"mem")) {
		print_page_faults();
		clean_page_table(pt);
	}
	return 0;
}
