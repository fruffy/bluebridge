#ifndef THRIFT_COMMON
#define THRIFT_COMMON
struct result {
  uint64_t alloc;
  uint64_t read;
  uint64_t write;
  uint64_t free;
  uint64_t rpc_start;
  uint64_t rpc_end;
};

FILE* generate_file_handle(const char * results_dir, char* method_name, char* operation, int size);
void get_args_pointer(struct in6_memaddr *ptr, struct sockaddr_in6 *targetIP);
struct in6_memaddr *get_args_pointers(struct sockaddr_in6 *targetIP, int num_pointers);
struct in6_memaddr get_result_pointer(struct sockaddr_in6 *targetIP);
void marshall_shmem_ptr(GByteArray **ptr, struct in6_memaddr *addr);
void unmarshall_shmem_ptr(struct in6_memaddr *result_addr, GByteArray *result_ptr);
void populate_array(uint8_t *array, int array_len, uint8_t start_num, gboolean random);
#endif