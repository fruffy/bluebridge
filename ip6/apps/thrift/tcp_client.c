#include <stdio.h>
#include <glib-object.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <math.h>

#include <thrift/c_glib/protocol/thrift_binary_protocol.h>
#include <thrift/c_glib/transport/thrift_buffered_transport.h>
#include <thrift/c_glib/transport/thrift_socket.h>
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

static const char *RESULTS_DIR = "results/thrift/tcp";
#define DATA_POINTS 1000
static int SIZE_STEPS[DATA_POINTS];

struct result test_increment_array(SimpleArrCompIf *client, int size, gboolean print) {
  struct result res;
  GError *error = NULL;                       // Error (in transport, socket, etc.)
  CallException *exception = NULL;            // Exception (thrown by server)
  GByteArray *result_arr = NULL;            // Result pointer
  int arr_len = size;                         // Size of array to be sent
  uint8_t incr_val = 1;                       // Value to increment each value in the array by
  GByteArray *arr = g_byte_array_new();       // Array to be sent (must be uint8_t to match char size)
  if (print)
    printf("Testing increment_array...\n");

  // Create argument array
  uint8_t *temp = malloc(arr_len);
  populate_array(temp, arr_len, 0, FALSE);

  g_byte_array_append(arr, temp, arr_len);
  // CALL RPC
  res.rpc_start = getns();
  simple_arr_comp_if_increment_array(client, &result_arr, arr, incr_val, arr_len, &exception, &error);
  // simple_arr_comp_if_increment_array(client, &result_ptr, args_ptr, incr_val, arr_len, &exception, &error);
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

  if (print) {
    // Make sure the server returned the correct result
    for (int i = 0; i < arr_len; i++) {
      if (g_array_index(result_arr, uint8_t, i) != g_array_index(arr, uint8_t, i) + incr_val) {
        printf("FAILED: result (%d) does not match expected result (%d) at index (%d)\n", g_array_index(result_arr, uint8_t, i), g_array_index(arr, uint8_t, i) + incr_val, i);
      }
    }
  }

  // Free malloc'd and GByteArray memory
  free(temp);
  g_byte_array_free (arr,TRUE);
  g_byte_array_free (result_arr,TRUE);
  if (print)
    printf("SUCCESS\n");

  return res;
}

struct result test_add_arrays(SimpleArrCompIf *client, int size, gboolean print) {
  GError *error = NULL;                       // Error (in transport, socket, etc.)
  CallException *exception = NULL;            // Exception (thrown by server)
  int arrays_len = size;                      // Size of array to be sent
  GByteArray* array1 = g_byte_array_new();    // Argument pointer
  GByteArray* array2 = g_byte_array_new();    // Argument pointer
  GByteArray* result_arr = NULL;            // Result pointer
  struct result res;

  if (print)
    printf("Testing add_arrays...\t\t");

  // Populate arrays
  uint8_t *temp1 = malloc(arrays_len);
  uint8_t *temp2 = malloc(arrays_len);
  populate_array(temp1, arrays_len, 3, TRUE);
  populate_array(temp2, arrays_len, 5, TRUE);
  g_byte_array_append(array1, temp1, arrays_len);
  g_byte_array_append(array2, temp2, arrays_len);

  res.rpc_start = getns();
  // CALL RPC
  simple_arr_comp_if_add_arrays(client, &result_arr, array1, array2, arrays_len, &exception, &error);
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
    if (print) {
      // Make sure the server returned the correct result
      uint8_t expected = 0;
      for (int i = 0; i < arrays_len; i++) {
        expected = temp1[i] + temp2[i];
        if ((uint8_t) result_arr->data[i] != (uint8_t) (temp1[i] + temp2[i])) {
          printf("FAILED: result (%d) does not match expected result (%d) at index (%d)\n", (uint8_t) result_arr->data[i], expected, i);
        }
      }

      printf("SUCCESS\n");
    }
  }
  // Free malloc'd and GByteArray memory
  free(temp1);
  free(temp2);
  g_byte_array_free (array1,TRUE);
  g_byte_array_free (array2,TRUE);
  g_byte_array_free (result_arr,TRUE);
  return res;
}

gint8 dot_prod_helper(GArray* x, const GArray* y, int m) {
  gint8 res = 0;

  for (int i = 0; i < m; i++) {
    res += g_array_index(x, gint8, i) + g_array_index(y, gint8, i);
  }

  return res;
}

// void mat_multiply(SimpleArrCompIf *client, gboolean print) {
//   GError *error = NULL;                       // Error (in transport, socket, etc.)
//   CallException *exception = NULL;            // Exception (thrown by server)
//   int32_t length = 10;
//   tuple* dimension = g_object_new(TYPE_TUPLE,
//                                  "n", 15,
//                                  "m", length,
//                                  NULL);
//   GArray* vector = g_array_sized_new(FALSE, FALSE, sizeof(uint8_t), length);  // Argument pointer
//   GPtrArray* matrix = g_ptr_array_sized_new(dimension->n);
//   GArray* result_vector = g_array_new(FALSE, FALSE, sizeof(uint8_t));              // Result pointer

//   if(print) {
//     printf("Testing matrix multiply...");
//   }

//   // Populate vector
//   uint8_t *temp = malloc(length*sizeof(uint8_t));
//   populate_array(temp, length, 1, FALSE);
//   g_array_append_vals(vector, temp, length);

//   // Populate Matrix
//   for (int i = 0; i < dimension->n; i++) {
//     uint8_t *vec = malloc(length*sizeof(uint8_t));
//     populate_array(vec, length, i, FALSE);
//     GArray* temp_row = g_array_sized_new(FALSE, FALSE, sizeof(uint8_t), length);
//     g_array_append_vals(temp_row, vec, length);
//     g_ptr_array_add(matrix, temp_row);
//     g_array_ref(temp_row);
//   }

//   // for (int i = 0; i < dimension->n; i++) {
//   //   GArray* row = g_ptr_array_index(matrix, i);
//   //   for (int j = 0; j < dimension->m; j++) {
//   //     printf("%d,", g_array_index(row, uint8_t, j));
//   //   }
//   //   printf("\n");
//   // }

//   simple_arr_comp_if_mat_multiply (client, &result_vector, vector, matrix, length, dimension, &exception, &error);
//   if (error) {
//     printf ("ERROR: %s\n", error->message);
//     g_clear_error (&error);
//   } else if (exception) {
//     gint32 err_code;
//     gchar *message;

//     g_object_get(exception, "message", &message,
//                             "err_code", &err_code,
//                             NULL);

//     // TODO: make a print macro that changes the message based on the error_code
//     printf("EXCEPTION %d: %s\n", err_code, message);
//   } else {
//     if (print) {
//       // Make sure the server returned the correct result
//       for (int i = 0; i < dimension->n; i++) {
//         gint8 expected = dot_prod_helper(g_ptr_array_index(matrix, i), vector, dimension->m);
//         gint8 got = g_array_index(result_vector, gint8, i);
//         if (got != expected) {
//           printf("FAILED: result (%d) does not match expected result (%d) at index (%d)\n", got, expected, i);
//         }
//       }

//       printf("\tSUCCESS\n");
//     }
//   }

// }

void word_count(SimpleArrCompIf *client) {
  // GError *error = NULL;                       // Error (in transport, socket, etc.)
  // CallException *exception = NULL;            // Exception (thrown by server)
  // GArray* args_ptr = g_array_sized_new(FALSE, FALSE, sizeof(char), length);  // Argument pointer
  // gint32* count_res = 0;              // Result pointer
  THRIFT_UNUSED_VAR(client);
}

void sort_array(SimpleArrCompIf *client) {
  // GError *error = NULL;                       // Error (in transport, socket, etc.)
  // CallException *exception = NULL;            // Exception (thrown by server)
  // GArray* args_ptr = g_array_sized_new(FALSE, FALSE, sizeof(uint8_t), length);  // Argument pointer
  // GArray* result_ptr = g_array_new(FALSE, FALSE, sizeof(uint8_t));              // Result pointer
  THRIFT_UNUSED_VAR(client);
}

uint64_t no_op_rpc(SimpleArrCompIf *client, int size) {
  GError *error = NULL;                       // Error (in transport, socket, etc.)
  int arr_len = size;                           // Size of array to be sent
  GByteArray* arr = g_byte_array_new();                       // Argument pointer
  GByteArray* result_arr = NULL;              // Result pointer
  THRIFT_UNUSED_VAR(client);
  // Create argument array
  uint8_t *temp = malloc(arr_len);
  populate_array(temp, arr_len, 0, FALSE);

  g_byte_array_append(arr, temp, arr_len);

  // CALL RPC
  uint64_t start = getns();
  simple_arr_comp_if_no_op(client, &result_arr, arr, arr_len, &error);
  uint64_t rpc_time = getns() - start;
  if (error) {
    printf ("ERROR: %s\n", error->message);
    g_clear_error (&error);
  }
  free(temp);
  return rpc_time;
}

void test_shared_pointer_rpc(SimpleArrCompIf *client) {
  test_increment_array(client, BLOCK_SIZE, TRUE);
  test_add_arrays(client, BLOCK_SIZE, TRUE);
  // mat_multiply(client, TRUE);
  // word_count(client, TRUE);
  // sort_array(client, TRUE);
}

void no_op_perf(SimpleArrCompIf *client, int iterations) {
  uint64_t *no_op_rpc_times = malloc(iterations*sizeof(uint64_t));
  uint64_t no_op_rpc_total = 0;
  for (int i = 0; i < iterations; i++) {
    no_op_rpc_times[i] = no_op_rpc(client, BLOCK_SIZE);
    no_op_rpc_total += no_op_rpc_times[i];
  }

  printf("Average %s latency: "KRED"%lu us\n"RESET, "no_op_rpcs", no_op_rpc_total / (iterations*1000));

  free(no_op_rpc_times);
}

void increment_array_perf(SimpleArrCompIf *client, int iterations, int max_size, int incr, char* method_name) {
  for (int s = 0; s < DATA_POINTS; s++) {
    printf("Starting Size %d with %d Iterations \n", SIZE_STEPS[s], iterations );
  //for (int s = 0; s < max_size; s+= incr) {
  //printf("Increment Array Size: %d\n", s );
    FILE* rpc_start_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_start", SIZE_STEPS[s]);
    FILE* rpc_end_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_end", SIZE_STEPS[s]);
    FILE* rpc_lat_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_lat", SIZE_STEPS[s]);
    FILE* send_file = generate_file_handle(RESULTS_DIR, method_name, "c1_send", SIZE_STEPS[s]);
    FILE* recv_file = generate_file_handle(RESULTS_DIR, method_name, "c1_recv", SIZE_STEPS[s]);
    for (int i = 0; i < iterations; i++) {
      struct result res = test_increment_array(client, SIZE_STEPS[s], FALSE);
      fprintf(rpc_start_file, "%lu\n", res.rpc_start);
      fprintf(rpc_end_file, "%lu\n", res.rpc_end);
      fprintf(rpc_lat_file, "%lu\n", res.rpc_end - res.rpc_start);
    }
    thrift_protocol_flush_timestamps(arrcomp_protocol, send_file, THRIFT_PERF_SEND, TRUE);
    thrift_protocol_flush_timestamps(arrcomp_protocol, recv_file, THRIFT_PERF_RECV, TRUE);
    fclose(rpc_start_file);
    fclose(rpc_end_file);
    fclose(rpc_lat_file);
    fclose(send_file);
    fclose(recv_file);
  }
}

void add_arrays_perf(SimpleArrCompIf *client, int iterations, int max_size, int incr, char* method_name) {
  for (int s = 0; s < DATA_POINTS; s++) {
    printf("Starting Size %d with %d Iterations \n", SIZE_STEPS[s], iterations );
  //for (int s = 0; s < max_size; s+= incr) {
    //printf("Add Array Size: %d\n", s );
    FILE* rpc_start_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_start", SIZE_STEPS[s]);
    FILE* rpc_end_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_end", SIZE_STEPS[s]);
    FILE* rpc_lat_file = generate_file_handle(RESULTS_DIR, method_name, "rpc_lat", SIZE_STEPS[s]);
    FILE* send_file = generate_file_handle(RESULTS_DIR, method_name, "c1_send", SIZE_STEPS[s]);
    FILE* recv_file = generate_file_handle(RESULTS_DIR, method_name, "c1_recv", SIZE_STEPS[s]);
    for (int i = 0; i < iterations; i++) {
      struct result res = test_add_arrays(client, SIZE_STEPS[s], FALSE);
      fprintf(rpc_start_file, "%lu\n", res.rpc_start);
      fprintf(rpc_end_file, "%lu\n", res.rpc_end);
      fprintf(rpc_lat_file, "%lu\n", res.rpc_end - res.rpc_start);
    }
    // socket flush of timestamps
    thrift_protocol_flush_timestamps(arrcomp_protocol, send_file, THRIFT_PERF_SEND, TRUE);
    thrift_protocol_flush_timestamps(arrcomp_protocol, recv_file, THRIFT_PERF_RECV, TRUE);
    fclose(rpc_start_file);
    fclose(rpc_end_file);
    fclose(rpc_lat_file);
    fclose(send_file);
    fclose(recv_file);
  }
}

void test_shared_pointer_perf(RemoteMemTestIf *remmem_client, SimpleArrCompIf *arrcomp_client, int iterations, int max_size, int incr) {
  microbenchmark_perf(remmem_client, iterations);

  printf("Starting no-op performance test...\n");
  // Call perf test for no-op RPC
  no_op_perf(arrcomp_client, iterations);

  thrift_protocol_flush_timestamps(arrcomp_protocol, NULL, THRIFT_PERF_SEND, FALSE);
  thrift_protocol_flush_timestamps(arrcomp_protocol, NULL, THRIFT_PERF_RECV, FALSE);

  printf("Starting increment array performance test...\n");
  // Call perf test for increment array rpc
  increment_array_perf(arrcomp_client, iterations, max_size, incr, "incr_arr");

  printf("Starting add arrays performance test...\n");
  // Call perf test for add arrays
  add_arrays_perf(arrcomp_client, iterations, max_size, incr, "add_arr");
}


void usage(char* prog_name, char* message) {
  if (strlen(message) > 0)
    printf("ERROR: %s\n", message);
  printf("USAGE: %s -c <config> -i <num_iterations>\n", prog_name);
  printf("Where");
  printf("\t-i <num_iterations is required and the number of iterations to run the performance test.\n");
}

int main (int argc, char *argv[]) {
  ThriftSocket *remmem_socket;
  ThriftTransport *remmem_transport;
  RemoteMemTestIf *remmem_client;

  ThriftSocket *arrcomp_socket;
  ThriftTransport *arrcomp_transport;
  SimpleArrCompIf *arrcomp_client;

  GError *error = NULL;

  int c; 
  struct config myConf;
  struct sockaddr_in6 *targetIP;
  int iterations = 0;
  int max_size = BLOCK_SIZE * 8;
  int incr = 100;

  while ((c = getopt (argc, argv, "c:i:m:s:")) != -1) { 
  switch (c) 
    { 
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

#if (!GLIB_CHECK_VERSION (2, 36, 0))
  g_type_init ();
#endif

  remmem_socket    = g_object_new (THRIFT_TYPE_SOCKET,
                            "hostname",  "102::",
                            "port",      9080,
                            NULL);
  remmem_transport = g_object_new (THRIFT_TYPE_BUFFERED_TRANSPORT,
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
  int enable =1;
  remmem_client = g_object_new (TYPE_REMOTE_MEM_TEST_CLIENT,
                                "input_protocol",  remmem_protocol,
                                "output_protocol", remmem_protocol,
                                NULL);
  
  arrcomp_socket    = g_object_new (THRIFT_TYPE_SOCKET,
                            "hostname",  "103::",
                            "port",      9280,
                            NULL);
  arrcomp_transport = g_object_new (THRIFT_TYPE_BUFFERED_TRANSPORT,
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

  /* Clean up previous files */
  int MAX_FNAME = 256;
  char cmd[MAX_FNAME];
  snprintf(cmd, MAX_FNAME, "rm -rf %s", RESULTS_DIR);
  printf("\nDeleting dir %s...", RESULTS_DIR);
  system(cmd);

  // Fill the global array with data values
  for (int i = 0; i < DATA_POINTS; i++ ) {
    SIZE_STEPS[i] = i*10;//pow(2,i);
  }

  arrcomp_client = g_object_new (TYPE_SIMPLE_ARR_COMP_CLIENT,
                                 "input_protocol",  arrcomp_protocol,
                                 "output_protocol", arrcomp_protocol,
                                 NULL);

  printf("\n\n###### Server functionality tests ######\n");
  test_server_functionality(remmem_client);

  printf("\n####### Shared pointer RPC tests #######\n");
  test_shared_pointer_rpc(arrcomp_client);

  printf("\n####### Shared pointer performance tests #######\n");
  test_shared_pointer_perf(remmem_client, arrcomp_client, iterations, max_size, incr);

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
