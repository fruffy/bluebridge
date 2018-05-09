#include <stdio.h>
#include <glib-object.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#include <thrift/c_glib/protocol/thrift_binary_protocol.h>
#include <thrift/c_glib/transport/thrift_buffered_udp_transport.h>
#include <thrift/c_glib/transport/thrift_udp_socket.h>
#include <thrift/c_glib/thrift.h>

#include "gen-c_glib/remote_mem_test.h"
#include "gen-c_glib/simple_arr_comp.h"
#include "lib/client_lib.h"
#include "lib/config.h"
#include "lib/utils.h"
#include "thrift_utils.h"
#include "mem_tests.h"


ThriftProtocol *remmem_protocol;
ThriftProtocol *arrcomp_protocol;
static const char *RESULTS_DIR = "results/thrift/ddc";
#define DATA_POINTS 24
static int SIZE_STEPS[DATA_POINTS];

struct result test_increment_array(SimpleArrCompIf *client, int size, struct sockaddr_in6 *targetIP, gboolean print) {
  GError *error = NULL;                       // Error (in transport, socket, etc.)
  CallException *exception = NULL;            // Exception (thrown by server)
  struct in6_memaddr args_addr;               // Shared memory address of the argument pointer
  GByteArray* args_ptr = g_byte_array_new();  // Argument pointer
  GByteArray* result_ptr = NULL;              // Result pointer
  int arr_len = size;                           // Size of array to be sent
  uint8_t *arr;                               // Array to be sent (must be uint8_t to match char size)
  uint8_t incr_val = 1;                       // Value to increment each value in the array by
  struct result res;
  uint64_t start;
  if (print)
    printf("Testing increment_array...\t");

  // Get a shared memory pointer for argument array
  // TODO: add individual timers here to get breakdown of costs. 
  start = getns();
  get_args_pointer(&args_addr, targetIP);
  res.alloc = getns() - start;

  // Create argument array
  arr = malloc(arr_len);
  populate_array(arr, arr_len, 0, FALSE);

  char temp[BLOCK_SIZE];
  if (arr_len > BLOCK_SIZE)
    memcpy(temp, arr, BLOCK_SIZE);
  else
    memcpy(temp, arr, arr_len);

  start = getns();
  // Write array to shared memory
  write_rmem(targetIP, (char*) temp, &args_addr);
  res.write = getns() - start;

  // Marshall shared pointer address
  marshall_shmem_ptr(&args_ptr, &args_addr);

  res.rpc_start = getns();
  // CALL RPC
  simple_arr_comp_if_increment_array(client, &result_ptr, args_ptr, incr_val, arr_len, &exception, &error);
  res.rpc_end = getns();

  if (print) {
    if (error) {
      printf ("ERROR: %s\n", error->message);
      g_clear_error (&error);
    } else if (exception) {
      gint32 err_code;
      gchar *message;

      g_object_get(exception, "message", &message,
                              "err_code", &err_code,
                              NULL);

      // TODO: make a print macro that changes the message based on the error_code
      printf("EXCEPTION %d: %s\n", err_code, message);
    }
  }

  // Unmarshall shared pointer address
  struct in6_memaddr result_addr;
  unmarshall_shmem_ptr(&result_addr, result_ptr);

  // Create result array to read into
  char* result_arr = malloc(arr_len);
  start = getns();
  // Read in result array
  if (arr_len > BLOCK_SIZE)
    get_rmem(result_arr, BLOCK_SIZE, targetIP, &result_addr);
  else
    get_rmem(result_arr, arr_len, targetIP, &result_addr);
  res.read = getns() - start;

  if (print) {
    // Make sure the server returned the correct result
    for (int i = 0; i < arr_len; i++) {
      if ((uint8_t) result_arr[i] != arr[i] + incr_val) {
        printf("FAILED: result (%d) does not match expected result (%d) at index (%d)\n", (uint8_t) result_arr[i], arr[i] + incr_val, i);
      }
    }
  }

  // Free malloc'd and GByteArray memory
  free(arr);
  free(result_arr);
  start = getns();
  free_rmem(targetIP, &args_addr);
  free_rmem(targetIP, &result_addr);
  res.free = getns() - start;
  g_byte_array_free(args_ptr, TRUE);  // We allocated this, so we free it
  g_byte_array_unref(result_ptr);     // We only received this, so we dereference it

  if (print)
    printf("SUCCESS\n");

  return res;
}

struct result test_add_arrays(SimpleArrCompIf *client, int size, struct sockaddr_in6 *targetIP, gboolean print) {
  GError *error = NULL;                       // Error (in transport, socket, etc.)
  CallException *exception = NULL;            // Exception (thrown by server)
  struct in6_memaddr arg1_addr;               // Shared memory address of the argument pointer
  struct in6_memaddr arg2_addr;               // Shared memory address of the argument pointer
  GByteArray* arg1_ptr = g_byte_array_new();  // Argument pointer
  GByteArray* arg2_ptr = g_byte_array_new();  // Argument pointer
  GByteArray* result_ptr = NULL;              // Result pointer
  struct in6_memaddr result_addr;             // Shared memory address of result
  int arr_len = size;                        // Size of array to be sent
  uint8_t *arr1;                              // Array 1 to be added
  uint8_t *arr2;                              // Array 2 to be added
  struct result res;
  uint64_t start;
  res.free = 0;

  if (print)
    printf("Testing add_arrays...\t\t");

  // Get pointers for arrays
  start = getns();
  get_args_pointer(&arg1_addr, targetIP);
  get_args_pointer(&arg2_addr, targetIP);
  res.alloc = getns() - start;

  // Populate arrays
  arr1 = malloc(arr_len);
  arr2 = malloc(arr_len);
  populate_array(arr1, arr_len, 3, TRUE);
  populate_array(arr2, arr_len, 5, TRUE);

  char temp1[BLOCK_SIZE];
  char temp2[BLOCK_SIZE];

  if (arr_len > BLOCK_SIZE){
    memcpy(temp1, arr1, BLOCK_SIZE);
    memcpy(temp2, arr2, BLOCK_SIZE);
  }
  else {
    memcpy(temp1, arr1, arr_len);
    memcpy(temp2, arr2, arr_len);
  }

  // Write arrays to shared memory
  start = getns();
  write_rmem(targetIP, (char*) temp1, &arg1_addr);
  write_rmem(targetIP, (char*) temp2, &arg2_addr);
  res.write = getns() - start;

  // Marshall shared pointer addresses
  marshall_shmem_ptr(&arg1_ptr, &arg1_addr);
  marshall_shmem_ptr(&arg2_ptr, &arg2_addr);

  // CALL RPC
  res.rpc_start = getns();
  simple_arr_comp_if_add_arrays(client, &result_ptr, arg1_ptr, arg2_ptr, arr_len, &exception, &error);
  res.rpc_end = getns();

  if (error) {
    printf ("ERROR: %s\n", error->message);
    g_clear_error (&error);
  } else if (exception) {
    gint32 err_code;
    gchar *message;

    g_object_get(exception, "message", &message,
                            "err_code", &err_code,
                            NULL);

    // TODO: make a print macro that changes the message based on the error_code
    printf("EXCEPTION %d: %s\n", err_code, message);
  } else {
    // Unmarshall shared pointer address
    unmarshall_shmem_ptr(&result_addr, result_ptr);

    // Create result array to read into
    char* result_arr = malloc(arr_len);

    // Read in result array
    start = getns();
    if (arr_len > BLOCK_SIZE)
      get_rmem(result_arr, BLOCK_SIZE, targetIP, &result_addr);
    else
      get_rmem(result_arr, arr_len, targetIP, &result_addr);
    res.read = getns() - start;

    if (print) {
      // Make sure the server returned the correct result
      uint8_t expected = 0;
      for (int i = 0; i < arr_len; i++) {
        expected = arr1[i] + arr2[i];
        if ((uint8_t) result_arr[i] != (uint8_t) (arr1[i] + arr2[i])) {
          printf("FAILED: result (%d) does not match expected result (%d) at index (%d)\n", (uint8_t) result_arr[i], expected, i);
          goto cleanupres;
        }
      }
      free(result_arr);
      printf("SUCCESS\n");
    }
cleanupres:
    start = getns();
    // free(arr1);
    // free(arr2);
    //free(result_arr);
    start = getns();
    free_rmem(targetIP, &result_addr);
    res.free += getns() - start;
  }

  // Free malloc'd and GByteArray memory
  start = getns();
  free(arr1);
  free(arr2);
  free_rmem(targetIP, &arg1_addr);    // Free the shared memory
  free_rmem(targetIP, &arg2_addr);    // Free the shared memory
  res.free += getns() - start;
  g_byte_array_free(arg1_ptr, TRUE);  // We allocated this, so we free it
  g_byte_array_free(arg2_ptr, TRUE);  // We allocated this, so we free it
  g_byte_array_unref(result_ptr);     // We only received this, so we dereference it

  return res;
}

void mat_multiply(SimpleArrCompIf *client, struct sockaddr_in6 *targetIP) {
  // GError *error = NULL;                       // Error (in transport, socket, etc.)
  // CallException *exception = NULL;            // Exception (thrown by server)
  // struct in6_memaddr args_addr;               // Shared memory address of the argument pointer
  // GByteArray* args_ptr = g_byte_array_new();  // Argument pointer
  // GByteArray* result_ptr = NULL;              // Result pointer

  THRIFT_UNUSED_VAR(client);
  THRIFT_UNUSED_VAR(targetIP);
}

void word_count(SimpleArrCompIf *client, struct sockaddr_in6 *targetIP) {
  // GError *error = NULL;                       // Error (in transport, socket, etc.)
  // CallException *exception = NULL;            // Exception (thrown by server)
  // struct in6_memaddr args_addr;               // Shared memory address of the argument pointer
  // GByteArray* args_ptr = g_byte_array_new();  // Argument pointer
  // GByteArray* result_ptr = NULL;              // Result pointer
  THRIFT_UNUSED_VAR(client);
  THRIFT_UNUSED_VAR(targetIP);
}

void sort_array(SimpleArrCompIf *client, struct sockaddr_in6 *targetIP) {
  // GError *error = NULL;                       // Error (in transport, socket, etc.)
  // CallException *exception = NULL;            // Exception (thrown by server)
  // struct in6_memaddr args_addr;               // Shared memory address of the argument pointer
  // GByteArray* args_ptr = g_byte_array_new();  // Argument pointer
  // GByteArray* result_ptr = NULL;              // Result pointer
  THRIFT_UNUSED_VAR(client);
  THRIFT_UNUSED_VAR(targetIP);
}

uint64_t no_op_rpc(SimpleArrCompIf *client, int size, struct sockaddr_in6 *targetIP) {
  GError *error = NULL;                       // Error (in transport, socket, etc.)
  struct in6_memaddr args_addr;               // Shared memory address of the argument pointer
  GByteArray* args_ptr = g_byte_array_new();  // Argument pointer
  GByteArray* result_ptr = NULL;              // Result pointer
  int arr_len = size;                           // Size of array to be sent
  uint8_t *arr;                               // Array to be sent (must be uint8_t to match char size)
  uint64_t start, rpc_time;

  //int num_pointers = (arr_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
  // Get a shared memory pointer for argument array
  get_args_pointer(&args_addr, targetIP);
  //args_addr = get_args_pointers(targetIP, num_pointers);
  // Create argument array
  arr = malloc(arr_len);
  populate_array(arr, arr_len, 0, FALSE);

  char temp[BLOCK_SIZE];
  if (arr_len > BLOCK_SIZE)
    memcpy(temp, arr, BLOCK_SIZE);
  else
    memcpy(temp, arr, arr_len);

  // Write array to shared memory
  write_rmem(targetIP, (char*) temp, &args_addr);
  //write_rmem_bulk(targetIP, (char*) temp, args_addr, num_pointers);
  // Marshall shared pointer address
  marshall_shmem_ptr(&args_ptr, &args_addr);

  start = getns();
  // CALL RPC
  simple_arr_comp_if_no_op(client, &result_ptr, args_ptr, arr_len, &error);
  rpc_time = getns() - start;

  // Unmarshall shared pointer address
  struct in6_memaddr result_addr;
  unmarshall_shmem_ptr(&result_addr, result_ptr);

  // Create result array to read into
  char *result_arr = malloc(arr_len);

  // Read in result array
  if (arr_len > BLOCK_SIZE)
    get_rmem(result_arr, BLOCK_SIZE, targetIP, &result_addr);
  else
    get_rmem(result_arr, arr_len, targetIP, &result_addr);

  // Free malloc'd and GByteArray memory
  free(arr);
  free(result_arr);
  free_rmem(targetIP, &args_addr);
  free_rmem(targetIP, &result_addr);
  // g_byte_array_free(args_ptr, TRUE);  // We allocated this, so we free it
  // g_byte_array_unref(result_ptr);     // We only received this, so we dereference it

  return rpc_time;
}

void test_shared_pointer_rpc(SimpleArrCompIf *client, struct sockaddr_in6 *targetIP) {
  test_increment_array(client, BLOCK_SIZE, targetIP, TRUE);
  test_add_arrays(client, BLOCK_SIZE, targetIP, TRUE);
  // mat_multiply(client, targetIP);
  // word_count(client, targetIP);
  // sort_array(client, targetIP);
}

void increment_array_perf(SimpleArrCompIf *client, 
                          struct sockaddr_in6 *targetIP, int iterations, 
                          int max_size, int incr, char* method_name) {

  uint64_t alloc_times = 0, read_times = 0, write_times = 0, free_times = 0;
  FILE* alloc_file = generate_file_handle(RESULTS_DIR, method_name, "alloc", -1);
  FILE* read_file = generate_file_handle(RESULTS_DIR, method_name, "read", -1);
  FILE* write_file = generate_file_handle(RESULTS_DIR, method_name, "write", -1);
  FILE* free_file = generate_file_handle(RESULTS_DIR, method_name, "free", -1);
  fprintf(alloc_file, "size,avg latency\n");
  fprintf(read_file, "size,avg latency\n");
  fprintf(write_file, "size,avg latency\n");
  fprintf(free_file, "size,avg latency\n");

  for (int s = 0; s < DATA_POINTS; s++) {
  //for (int s = 0; s < max_size; s+= incr) {
    FILE* rpc_start_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_start", SIZE_STEPS[s]);
    FILE* rpc_end_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_end", SIZE_STEPS[s]);
    FILE* rpc_lat_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_lat", SIZE_STEPS[s]);
    FILE* send_file = generate_file_handle(RESULTS_DIR, method_name, "c1_send", SIZE_STEPS[s]);
    FILE* recv_file = generate_file_handle(RESULTS_DIR, method_name, "c1_recv", SIZE_STEPS[s]);
    alloc_times = 0;
    read_times = 0;
    write_times = 0;
    free_times = 0;
    for (int i = 0; i < iterations; i++) {
      struct result res = test_increment_array(client, SIZE_STEPS[s], targetIP, FALSE);
      alloc_times += res.alloc;
      read_times += res.read;
      write_times += res.write;
      free_times += res.free;
      fprintf(rpc_start_file, "%lu\n", res.rpc_start);
      fprintf(rpc_end_file, "%lu\n", res.rpc_end);
      fprintf(rpc_lat_file, "%lu\n", res.rpc_end - res.rpc_start);
    }
    fprintf(alloc_file, "%d,%lu\n", SIZE_STEPS[s], alloc_times / (iterations*1000) );
    fprintf(read_file, "%d,%lu\n", SIZE_STEPS[s], read_times / (iterations*1000) );
    fprintf(write_file, "%d,%lu\n", SIZE_STEPS[s], write_times / (iterations*1000) );
    fprintf(free_file, "%d,%lu\n", SIZE_STEPS[s], free_times / (iterations*1000) );
    thrift_protocol_flush_timestamps(arrcomp_protocol, send_file, THRIFT_PERF_SEND, TRUE);
    thrift_protocol_flush_timestamps(arrcomp_protocol, recv_file, THRIFT_PERF_RECV, TRUE);
    fclose(send_file);
    fclose(recv_file);
    fclose(rpc_start_file);
    fclose(rpc_end_file);
    fclose(rpc_lat_file);
  }
  fclose(alloc_file);
  fclose(read_file);
  fclose(write_file);
  fclose(free_file);
}

void add_arrays_perf(SimpleArrCompIf *client, 
                     struct sockaddr_in6 *targetIP, int iterations, 
                     int max_size, int incr, char* method_name) {
  uint64_t alloc_times = 0, read_times = 0, write_times = 0, free_times = 0;
  FILE* alloc_file = generate_file_handle(RESULTS_DIR, method_name, "alloc", -1);
  FILE* read_file = generate_file_handle(RESULTS_DIR, method_name, "read", -1);
  FILE* write_file = generate_file_handle(RESULTS_DIR, method_name, "write", -1);
  FILE* free_file = generate_file_handle(RESULTS_DIR, method_name, "free", -1);
  fprintf(alloc_file, "size,avg latency\n");
  fprintf(read_file, "size,avg latency\n");
  fprintf(write_file, "size,avg latency\n");
  fprintf(free_file, "size,avg latency\n");

  for (int s = 0; s < DATA_POINTS; s++) {
  //for (int s = 0; s < max_size; s+= incr) {
    FILE* rpc_start_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_start", SIZE_STEPS[s]);
    FILE* rpc_end_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_end", SIZE_STEPS[s]);
    FILE* rpc_lat_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_lat", SIZE_STEPS[s]);
    FILE* send_file = generate_file_handle(RESULTS_DIR, method_name, "c1_send", SIZE_STEPS[s]);
    FILE* recv_file = generate_file_handle(RESULTS_DIR, method_name, "c1_recv", SIZE_STEPS[s]);
    alloc_times = 0;
    read_times = 0;
    write_times = 0;
    free_times = 0;
    for (int i = 0; i < iterations; i++) {
      struct result res = test_add_arrays(client, SIZE_STEPS[s], targetIP, FALSE);
      alloc_times += res.alloc;
      read_times += res.read;
      write_times += res.write;
      free_times += res.free;
      fprintf(rpc_start_file, "%lu\n", res.rpc_start);
      fprintf(rpc_end_file, "%lu\n", res.rpc_end);
      fprintf(rpc_lat_file, "%lu\n", res.rpc_end - res.rpc_start);
    }
    fprintf(alloc_file, "%d,%lu\n", SIZE_STEPS[s], alloc_times / (iterations*1000) );
    fprintf(read_file, "%d,%lu\n", SIZE_STEPS[s], read_times / (iterations*1000) );
    fprintf(write_file, "%d,%lu\n", SIZE_STEPS[s], write_times / (iterations*1000) );
    fprintf(free_file, "%d,%lu\n", SIZE_STEPS[s], free_times / (iterations*1000) );
    thrift_protocol_flush_timestamps(arrcomp_protocol, send_file, THRIFT_PERF_SEND, TRUE);
    thrift_protocol_flush_timestamps(arrcomp_protocol, recv_file, THRIFT_PERF_RECV, TRUE);
    fclose(send_file);
    fclose(recv_file);
    fclose(rpc_start_file);
    fclose(rpc_end_file);
    fclose(rpc_lat_file);
  }
  fclose(alloc_file);
  fclose(read_file);
  fclose(write_file);
  fclose(free_file);
}

void no_op_perf(SimpleArrCompIf *client, struct sockaddr_in6 *targetIP,
                int iterations, int max_size, int incr) {
  // uint64_t *no_op_rpc_times = malloc(iterations*sizeof(uint64_t));
  uint64_t no_op_rpc_total;
  int s = BLOCK_SIZE;

  // for (int s = 10; s < max_size; s+= incr) {
    no_op_rpc_total = 0;
    for (int i = 0; i < iterations; i++) {
      no_op_rpc_total += no_op_rpc(client, s, targetIP);
       // no_op_rpc_times[i];
    }
    printf("Average %s latency (%d): "KRED"%lu us\n"RESET, "no_op_rpcs", s, no_op_rpc_total / (iterations*1000));
  // }
  // free(no_op_rpc_times);
}

void test_shared_pointer_perf(RemoteMemTestIf *remmem_client, 
                              SimpleArrCompIf *arrcomp_client, 
                              struct sockaddr_in6 *targetIP, int iterations,
                              int max_size, int incr) {
  microbenchmark_perf(remmem_client, iterations);

  // TODO: debug, only the first one of these will work consistently, then the server seg faults
  // on a write_rmem. We might be running out of memory somewhere?

  printf("Starting no-op performance test...\n");
  // Call perf test for no-op RPC
  no_op_perf(arrcomp_client, targetIP, iterations, max_size, incr);

  thrift_protocol_flush_timestamps(arrcomp_protocol, NULL, THRIFT_PERF_SEND, FALSE);
  thrift_protocol_flush_timestamps(arrcomp_protocol, NULL, THRIFT_PERF_RECV, FALSE);

  printf("Starting increment array performance test...\n");
  // Call perf test for increment array rpc
  increment_array_perf(arrcomp_client, targetIP, iterations, max_size, incr, "incr_arr");

  thrift_protocol_flush_timestamps(arrcomp_protocol, NULL, THRIFT_PERF_SEND, FALSE);
  thrift_protocol_flush_timestamps(arrcomp_protocol, NULL, THRIFT_PERF_RECV, FALSE);

  printf("Starting add arrays performance test...\n");
  // Call perf test for add arrays
  add_arrays_perf(arrcomp_client, targetIP, iterations, max_size, incr, "add_arr");
}

void usage(char* prog_name, char* message) {
  if (strlen(message) > 0)
    printf("ERROR: %s\n", message);
  printf("USAGE: %s -c <config> -i <num_iterations>\n", prog_name);
  printf("Where");
  printf("\t-c <config> is required and the bluebridge config (generated by mininet)\n");
  printf("\t-i <num_iterations is required and the number of iterations to run the performance test.\n");
}

int main (int argc, char *argv[]) {
  ThriftUDPSocket *remmem_socket;
  ThriftTransport *remmem_transport;
  RemoteMemTestIf *remmem_client;

  ThriftUDPSocket *arrcomp_socket;
  ThriftTransport *arrcomp_transport;
  SimpleArrCompIf *arrcomp_client;

  GError *error = NULL;

  int c; 
  struct config myConf;
  struct sockaddr_in6 *targetIP;
  int iterations = 0;
  int max_size = BLOCK_SIZE * 8;
  int incr = 100;

  if (argc < 5) {
    usage(argv[0], "Not enough arguments");
    return -1;
  }

  while ((c = getopt (argc, argv, "c:i:m:s:")) != -1) { 
  switch (c) 
    { 
    case 'c':
      myConf = set_bb_config(optarg, 0);
      break;
    case 'i':
      iterations = atoi(optarg);
      break;
    case 'm':
      max_size = atoi(optarg);
      break;
    case 's':
      incr = atoi(optarg);
      break;
    case '?':
      usage(argv[0], "");
      return 0; 
    default: ; // b/c cannot create variable after label
      char *message = malloc(180);
      snprintf (message, 180, "Unknown option `-%c'.\n", optopt);
      usage(argv[0], message);
      free(message);
      return -1;
    } 
  }

  targetIP = init_sockets(&myConf, 0);
  set_host_list(myConf.hosts, myConf.num_hosts);

#if (!GLIB_CHECK_VERSION (2, 36, 0))
  g_type_init ();
#endif

  remmem_socket    = g_object_new (THRIFT_TYPE_UDP_SOCKET,
                            "hostname",  "102::",
                            "port",      9080,
                            NULL);
  remmem_transport = g_object_new (THRIFT_TYPE_BUFFERED_UDP_TRANSPORT,
                            "transport", remmem_socket,
                            NULL);
  remmem_protocol  = g_object_new (THRIFT_TYPE_BINARY_PROTOCOL,
                            "transport", remmem_transport,
                            NULL);

  thrift_transport_open (remmem_transport, &error);
  if (error) {
    printf ("ERROR: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }

  remmem_client = g_object_new (TYPE_REMOTE_MEM_TEST_CLIENT,
                                "input_protocol",  remmem_protocol,
                                "output_protocol", remmem_protocol,
                                NULL);
  
  arrcomp_socket    = g_object_new (THRIFT_TYPE_UDP_SOCKET,
                            "hostname",  "103::",
                            "port",      9180,
                            NULL);
  arrcomp_transport = g_object_new (THRIFT_TYPE_BUFFERED_UDP_TRANSPORT,
                            "transport", arrcomp_socket,
                            NULL);
  arrcomp_protocol  = g_object_new (THRIFT_TYPE_BINARY_PROTOCOL,
                            "transport", arrcomp_transport,
                            NULL);

  thrift_transport_open (arrcomp_transport, &error);
  if (error) {
    printf ("ERROR: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }

  arrcomp_client = g_object_new (TYPE_SIMPLE_ARR_COMP_CLIENT,
                                 "input_protocol",  arrcomp_protocol,
                                 "output_protocol", arrcomp_protocol,
                                 NULL);

  /* Clean up previous files */
  int MAX_FNAME = 256;
  char cmd[MAX_FNAME];
  snprintf(cmd, MAX_FNAME, "rm -rf %s", RESULTS_DIR);
  printf("\nDeleting dir %s...", RESULTS_DIR);
  system(cmd);

  // Fill the global array with data values
  for (int i = 0; i < DATA_POINTS; i++ ) {
    SIZE_STEPS[i] = pow(2,i);
  }

  printf("\n\n###### Server functionality tests ######\n");
  test_server_functionality(remmem_client);

  printf("\n####### Shared pointer RPC tests #######\n");
  test_shared_pointer_rpc(arrcomp_client, targetIP);

  printf("\n####### Shared pointer performance tests #######\n");
  test_shared_pointer_perf(remmem_client, arrcomp_client, targetIP, iterations, max_size, incr);

  printf("\n\nCleaning up...\n");
  thrift_transport_close (remmem_transport, NULL);
  thrift_transport_close (arrcomp_transport, NULL);

  g_object_unref (remmem_client);
  g_object_unref (remmem_protocol);
  g_object_unref (remmem_transport);
  g_object_unref (remmem_socket);

  g_object_unref (arrcomp_client);
  g_object_unref (arrcomp_protocol);
  g_object_unref (arrcomp_transport);
  g_object_unref (arrcomp_socket);

  return 0;
}
