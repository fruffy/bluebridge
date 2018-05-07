#include <stdio.h>
#include <glib-object.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <thrift/c_glib/protocol/thrift_binary_protocol.h>
#include <thrift/c_glib/transport/thrift_buffered_udp_transport.h>
#include <thrift/c_glib/transport/thrift_udp_socket.h>
#include <thrift/c_glib/thrift.h>

#include "lib/client_lib.h"
#include "lib/config.h"
#include "lib/utils.h"
#include "thrift_utils.h"

gboolean srand_called = FALSE;

void create_dir(const char *name) {
    struct stat st = {0};
    if (stat(name, &st) == -1) {
        int MAX_FNAME = 256;
        char fname[MAX_FNAME];
        snprintf(fname, MAX_FNAME, "mkdir -p %s -m 777", name);
        printf("Creating dir %s...\n", name);
        system(fname);
    }
}

FILE* generate_file_handle(const char * results_dir, char* method_name, char* operation, int size) {
  int wrote = 0;
  char handle[512];
  create_dir(results_dir);
  if (size > -1) {
    wrote = snprintf(handle, 512, "%s/%s_%s_%d.txt", results_dir, method_name, operation, size);
  } else {
    wrote = snprintf(handle, 512, "%s/%s_%s.txt", results_dir, method_name, operation);
  }
  return fopen(handle, "w");
}

// UTILS --> copied to server b/c it's a pain to create a shared file (build issues)
void get_args_pointer(struct in6_memaddr *ptr, struct sockaddr_in6 *targetIP) {
  // Get random memory server
  struct in6_addr *ipv6Pointer = gen_rdm_ip6_target();

  // Put it's address in targetIP (why?)
  memcpy(&(targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));

  // Allocate memory and receive the remote memory pointer
  struct in6_memaddr temp = allocate_rmem(targetIP);

  // Copy the remote memory pointer into the give struct pointer
  memcpy(ptr, &temp, sizeof(struct in6_memaddr));
}

void get_result_pointer(struct sockaddr_in6 *targetIP, struct in6_memaddr *ptr) {
  // Get random memory server
  struct in6_addr *ipv6Pointer = gen_rdm_ip6_target();

  // Put it's address in targetIP (why?)
  memcpy(&(targetIP->sin6_addr), ipv6Pointer, sizeof(*ipv6Pointer));

  // Allocate memory and receive the remote memory pointer
  struct in6_memaddr temp = allocate_rmem(targetIP);

  // Copy the remote memory pointer into the give struct pointer
  memcpy(ptr, &temp, sizeof(struct in6_memaddr));
}

void marshall_shmem_ptr(GByteArray **ptr, struct in6_memaddr *addr) {
  // Blank cmd section
  //uint16_t cmd = 0u;

  // Copy subid (i.e., 103)
  *ptr = g_byte_array_append(*ptr, (const gpointer) &(addr->subid), sizeof(uint16_t));
  // Copy cmd (0)
  *ptr = g_byte_array_append(*ptr, (const gpointer) &addr->cmd, sizeof(uint16_t));
  // Copy args ()
  *ptr = g_byte_array_append(*ptr, (const gpointer) &(addr->args), sizeof(uint32_t));
  // Copy memory address (XXXX:XXXX)
  *ptr = g_byte_array_append(*ptr, (const gpointer) &(addr->paddr), sizeof(uint64_t));
}

void unmarshall_shmem_ptr(struct in6_memaddr *result_addr, GByteArray *result_ptr) {
  // Clear struct
  memset(result_addr, 0, sizeof(struct in6_memaddr));
  // Copy over received bytes
  memcpy(result_addr, result_ptr->data, sizeof(struct in6_memaddr));
}

void populate_array(uint8_t *array, int array_len, uint8_t start_num, gboolean random) {
  uint8_t num = start_num;
  if (!srand_called && random) {
    srand(time(NULL));
    srand_called = TRUE;
  }

  for (int i = 0; i < array_len; i++) {
    array[i] = num;
    if (!random) {
      num++;
      num = num % UINT8_MAX;
    } else {
      num = ((uint8_t) rand()) % UINT8_MAX;
    }
  }
}
