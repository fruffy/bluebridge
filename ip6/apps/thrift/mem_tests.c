#include <stdio.h>
#include <glib-object.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "lib/client_lib.h"
#include "lib/config.h"
#include "lib/utils.h"
#include "thrift_utils.h"

#include "gen-c_glib/remote_mem_test.h"
#include "gen-c_glib/simple_arr_comp.h"
// TESTS
uint64_t test_ping(RemoteMemTestIf *client, GError *error, gboolean print) {
  if(print)
    printf("Testing ping...\t\t\t");
  
  uint64_t ping_start = getns();
  gboolean success = remote_mem_test_if_ping (client, &error);
  uint64_t ping_time = getns() - ping_start;

  if (print){
    if (success) {
      printf("SUCCESS\n");
    } else {
      printf("FAILED\n");
      printf("\tERROR: %s\n", error->message);
      g_clear_error (&error);
    }
  }
  return ping_time;
}

uint64_t test_server_alloc(RemoteMemTestIf *client, GByteArray **res, CallException *exception, GError *error, gboolean print) {
  if (print)
    printf("Testing allocate_mem...\t\t");
  
  uint64_t alloc_start = getns();
  gboolean success = remote_mem_test_if_allocate_mem(client, res, BLOCK_SIZE, &exception, &error);
  uint64_t alloc_time = getns() - alloc_start;

  if (print) {
    if (success) {
      printf("SUCCESS\n");
    } else {
      // TODO add exception case
      printf("FAILED\n");
      printf("\tERROR: %s\n", error->message);
      g_clear_error (&error);
    }
  }
  return alloc_time;
}

uint64_t test_server_write(RemoteMemTestIf *client, GByteArray *res, CallException *exception, GError *error, gboolean print) {
  if (print)
    printf("Testing write_mem...\t\t");
  // Clear payload
  char *payload = malloc(BLOCK_SIZE);
  snprintf(payload, 50, "HELLO WORLD! How are you?");

  uint64_t write_start = getns();
  gboolean success = remote_mem_test_if_write_mem(client, res, payload, &exception, &error);
  uint64_t write_time = getns() - write_start;

  if (print) {
    if (success) {
      printf ("SUCCESS\n");
    } else {
      // TODO: add exception case
      printf("FAILED\n");
      printf("\tERROR: %s\n", error->message);
      g_clear_error (&error);
    }
  }

  free(payload);
  return write_time;
}

uint64_t test_server_read(RemoteMemTestIf *client, GByteArray *res, CallException *exception, GError *error, gboolean print) {
  if (print)
    printf("Testing read_mem...\t\t");
  
  uint64_t read_start = getns();
  gboolean success = remote_mem_test_if_read_mem(client, res, &exception, &error);
  uint64_t read_time = getns() - read_start;

  if (print) {
    if (success) {
      printf ("SUCCESS\n");
    } else {
      printf("FAILED\n");
      printf("\tERROR: %s\n", error->message);
      g_clear_error (&error);
    }
  }

  return read_time;
}

uint64_t test_server_free(RemoteMemTestIf *client, GByteArray *res, CallException *exception, GError *error, gboolean print) {
  if (print)
    printf("Testing free_mem...\t\t");
  
  uint64_t free_start = getns();
  gboolean success = remote_mem_test_if_free_mem(client, res, &exception, &error);
  uint64_t free_time = getns() - free_start;

  if (print) {
    if (success) {
      printf ("SUCCESS\n");
    } else {
      printf("FAILED\n");
      printf("\tERROR: %s\n", error->message);
      g_clear_error (&error);
    }
  }

  return free_time;
}

void test_server_functionality(RemoteMemTestIf *client) {
  GError *error = NULL;
  CallException *exception = NULL;
  GByteArray* res = NULL;

  test_ping(client, error, TRUE);

  test_server_alloc(client, &res, exception, error, TRUE);

  test_server_write(client, res, exception, error, TRUE);

  test_server_read(client, res, exception, error, TRUE);

  test_server_free(client, res, exception, error, TRUE);
}

void microbenchmark_perf(RemoteMemTestIf *client, int iterations) {
  GError *error = NULL;
  CallException *exception = NULL;

  // Call perf test for ping
  uint64_t *ping_times = malloc(iterations*sizeof(uint64_t));
  uint64_t ping_total = 0;
  uint64_t *alloc_times = malloc(iterations*sizeof(uint64_t));
  uint64_t alloc_total = 0;
  uint64_t *write_times = malloc(iterations*sizeof(uint64_t));
  uint64_t write_total = 0;
  uint64_t *read_times = malloc(iterations*sizeof(uint64_t));
  uint64_t read_total = 0;
  uint64_t *free_times = malloc(iterations*sizeof(uint64_t));
  uint64_t free_total = 0;

  for (int i = 0; i < iterations; i++) {
    GByteArray* res = NULL;

    ping_times[i] = test_ping(client, error, FALSE);
    ping_total += ping_times[i];

    alloc_times[i] = test_server_alloc(client, &res, exception, error, FALSE);
    alloc_total += alloc_times[i];

    write_times[i] = test_server_write(client, res, exception, error, FALSE);
    write_total += write_times[i];

    read_times[i] = test_server_read(client, res, exception, error, FALSE);
    read_total += read_times[i];

    free_times[i] = test_server_free(client, res, exception, error, FALSE);
    free_total += free_times[i];
  }

  printf("Average %s latency: "KRED"%lu us\n"RESET, "ping", ping_total / (iterations*1000));
  printf("Average %s latency: "KRED"%lu us\n"RESET, "alloc", alloc_total / (iterations*1000));
  printf("Average %s latency: "KRED"%lu us\n"RESET, "write", write_total / (iterations*1000));
  printf("Average %s latency: "KRED"%lu us\n"RESET, "read", read_total / (iterations*1000));
  printf("Average %s latency: "KRED"%lu us\n"RESET, "free", free_total / (iterations*1000));

  free(ping_times);
  free(alloc_times);
  free(write_times);
  free(read_times);
  free(free_times);
}