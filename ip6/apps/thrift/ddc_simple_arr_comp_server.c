/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <glib-object.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>

#include <thrift/c_glib/thrift.h>
#include <thrift/c_glib/protocol/thrift_binary_protocol_factory.h>
#include <thrift/c_glib/protocol/thrift_protocol_factory.h>
#include <thrift/c_glib/server/thrift_server.h>
#include <thrift/c_glib/server/thrift_simple_udp_server.h>
#include <thrift/c_glib/transport/thrift_buffered_udp_transport_factory.h>
#include <thrift/c_glib/transport/thrift_udp_socket.h>
#include <thrift/c_glib/transport/thrift_buffered_udp_transport.h>

#include "gen-c_glib/simple_arr_comp.h"
#include "lib/client_lib.h"
#include "lib/config.h"
#include "lib/utils.h"
#include "thrift_utils.h"

G_BEGIN_DECLS

/* In the C (GLib) implementation of Thrift, the actual work done by a
   server---that is, the code that runs when a client invokes a
   service method---is defined in a separate "handler" class that
   implements the service interface. Here we define the
   TutorialSimpleArrCompHandler class, which implements the SimpleArrCompIf
   interface and provides the behavior expected by tutorial clients.
   (Typically this code would be placed in its own module but for
   clarity this tutorial is presented entirely in a single file.)

   For each service the Thrift compiler generates an abstract base
   class from which handler implementations should inherit. In our
   case TutorialSimpleArrCompHandler inherits from SimpleArrCompHandler,
   defined in gen-c_glib/simple_arr_comp.h.

   If you're new to GObject, try not to be intimidated by the quantity
   of code here---much of it is boilerplate and can mostly be
   copied-and-pasted from existing work. For more information refer to
   the GObject Reference Manual, available online at
   https://developer.gnome.org/gobject/. */

#define TYPE_TUTORIAL_SIMPLE_ARR_COMP_HANDLER \
  (tutorial_simple_arr_comp_handler_get_type ())

#define TUTORIAL_SIMPLE_ARR_COMP_HANDLER(obj)                                \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               TYPE_TUTORIAL_SIMPLE_ARR_COMP_HANDLER,        \
                               TutorialSimpleArrCompHandler))
#define TUTORIAL_SIMPLE_ARR_COMP_HANDLER_CLASS(c)                    \
  (G_TYPE_CHECK_CLASS_CAST ((c),                                \
                            TYPE_TUTORIAL_SIMPLE_ARR_COMP_HANDLER,   \
                            TutorialSimpleArrCompHandlerClass))
#define IS_TUTORIAL_SIMPLE_ARR_COMP_HANDLER(obj)                             \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               TYPE_TUTORIAL_SIMPLE_ARR_COMP_HANDLER))
#define IS_TUTORIAL_SIMPLE_ARR_COMP_HANDLER_CLASS(c)                 \
  (G_TYPE_CHECK_CLASS_TYPE ((c),                                \
                            TYPE_TUTORIAL_SIMPLE_ARR_COMP_HANDLER))
#define TUTORIAL_SIMPLE_ARR_COMP_HANDLER_GET_CLASS(obj)              \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                            \
                              TYPE_TUTORIAL_SIMPLE_ARR_COMP_HANDLER, \
                              TutorialSimpleArrCompHandlerClass))

struct _TutorialSimpleArrCompHandler {
  SimpleArrCompHandler parent_instance;

  /* private */
  GHashTable *log;
};
typedef struct _TutorialSimpleArrCompHandler TutorialSimpleArrCompHandler;

struct _TutorialSimpleArrCompHandlerClass {
  SimpleArrCompHandlerClass parent_class;
};
typedef struct _TutorialSimpleArrCompHandlerClass TutorialSimpleArrCompHandlerClass;

GType tutorial_simple_arr_comp_handler_get_type (void);

G_END_DECLS

struct sockaddr_in6 *targetIP;

/* ---------------------------------------------------------------- */

/* The implementation of TutorialSimpleArrCompHandler follows. */


G_DEFINE_TYPE (TutorialSimpleArrCompHandler,
               tutorial_simple_arr_comp_handler,
               TYPE_SIMPLE_ARR_COMP_HANDLER);

static gboolean
tutorial_simple_arr_comp_handler_increment_array (SimpleArrCompIf *iface,
                                                     GByteArray        **_return,
                                                     const GByteArray   *pointer,
                                                     const gint8         value,
                                                     const gint32        length,
                                                     CallException     **ouch,
                                                     GError            **error) 
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);

  GByteArray* result_ptr = g_byte_array_new();
  struct in6_memaddr result_addr;

  get_result_pointer(targetIP, &result_addr);

  marshall_shmem_ptr(&result_ptr, &result_addr);

  // Read in array from shared memory
  uint8_t *int_arr = malloc(length);

  struct in6_memaddr args_addr;
  unmarshall_shmem_ptr(&args_addr, (GByteArray *) pointer);

  get_rmem((char *) int_arr, length, targetIP, &args_addr);

  // Increment the values
  for (int i = 0; i < length; i++) {
    int_arr[i] += value;
  }

  char temp[BLOCK_SIZE];

  memcpy(temp, int_arr, length);

  // Write it to the array
  write_rmem(targetIP, (char*) temp, &result_addr);

  g_byte_array_ref(result_ptr);

  *_return = result_ptr;

  // printf("increment_array() returning ");
  // print_n_bytes(result_ptr->data, result_ptr->len);

  free(int_arr);
  return TRUE;

}

static gboolean
tutorial_simple_arr_comp_handler_add_arrays (SimpleArrCompIf *iface,
                                                GByteArray        **_return,
                                                const GByteArray   *array1,
                                                const GByteArray   *array2,
                                                const gint32        length,
                                                CallException     **ouch,
                                                GError            **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);

  GByteArray* result_ptr = g_byte_array_new();
  struct in6_memaddr result_addr;

  // printf("Get result pointer\n");
  get_result_pointer(targetIP, &result_addr);

  // printf("marshall_shmem_ptr\n");
  marshall_shmem_ptr(&result_ptr, &result_addr);
  // printf("result pointer: ");
  // print_n_bytes(result_ptr->data, result_ptr->len);

  // Read in params from shared memory
  uint8_t *arr1 = malloc(length);
  uint8_t *arr2 = malloc(length);

  struct in6_memaddr arg1_addr;
  struct in6_memaddr arg2_addr;

  // printf("unmarshall_shmem_ptr\n");
  unmarshall_shmem_ptr(&arg1_addr, (GByteArray *) array1);
  unmarshall_shmem_ptr(&arg2_addr, (GByteArray *) array2);

  // printf("get_rmem\n");
  get_rmem((char*) arr1, length, targetIP, &arg1_addr);
  get_rmem((char*) arr2, length, targetIP, &arg2_addr);

  // Create result array
  uint8_t *result_array = malloc(length);

  // printf("Perform computation\n");
  // Perform computation
  for (int i = 0; i < length; i++) {
    result_array[i] = arr1[i] + arr2[i];
  }

  // printf("write_rmem\n");

  char temp[BLOCK_SIZE];

  memcpy(temp, result_array, length);
  // Write computation to shared memory
  write_rmem(targetIP, (char*) temp, &result_addr);

  // printf("increase ref\n");
  g_byte_array_ref(result_ptr);

  // printf ("add_arrays (%d): \n\t1: ", length);
  // print_n_bytes(array1->data, array1->len);
  // printf("\t2: ");
  // print_n_bytes(array2->data, array2->len);

  *_return = result_ptr;

  free(result_array);

  return TRUE;

}

static gboolean
tutorial_simple_arr_comp_handler_mat_multiply (SimpleArrCompIf *iface,
                                                  const GByteArray   *array,
                                                  const GByteArray   *matrix,
                                                  const gint32        length,
                                                  const tuple        *dimension,
                                                  const GByteArray   *result_ptr,
                                                  CallException     **ouch,
                                                  GError            **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);
  THRIFT_UNUSED_VAR (result_ptr);

  printf ("mat_multiply (length: %d, dimension: %d, %d): \n\tArray: ", length, dimension->n, dimension->m);
  print_n_bytes(array->data, array->len);
  printf("\tMatrix: ");
  print_n_bytes(matrix->data, matrix->len);

  return TRUE;

}

static gboolean
tutorial_simple_arr_comp_handler_word_count (SimpleArrCompIf *iface,
                                                gint32             *_return,
                                                const GByteArray   *story,
                                                const gint32        length,
                                                CallException     **ouch,
                                                GError            **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);

  *_return = -1;

  printf("word_count (%d): ", length);
  print_n_bytes(story->data, story->len);

  return TRUE;

}

static gboolean
tutorial_simple_arr_comp_handler_sort_array (SimpleArrCompIf *iface,
                                                GByteArray        **_return,
                                                const GByteArray   *num_array,
                                                const gint32        length,
                                                CallException     **ouch,
                                                GError            **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);

  GByteArray* res = g_byte_array_new();

  g_byte_array_ref(res);

  *_return = res;

  printf("sort_array(%d): ", length);
  print_n_bytes(num_array->data, num_array->len);

  return TRUE;

}

static gboolean
tutorial_simple_arr_comp_handler_no_op (SimpleArrCompIf  *iface,
                                           GByteArray         **_return,
                                           const GByteArray    *num_array,
                                           const gint32         length,
                                           GError             **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);

  GByteArray* result_ptr = g_byte_array_new();
  struct in6_memaddr result_addr;

  get_result_pointer(targetIP, &result_addr);

  marshall_shmem_ptr(&result_ptr, &result_addr);

  // Read in array from shared memory
  uint8_t *int_arr = malloc(length);

  struct in6_memaddr args_addr;
  unmarshall_shmem_ptr(&args_addr, (GByteArray *) num_array);

  get_rmem((char *) int_arr, length, targetIP, &args_addr);

  // COMPUTATION

  char temp[BLOCK_SIZE];

  memcpy(temp, int_arr, length);

  // Write it to the array
  write_rmem(targetIP, (char*) temp, &result_addr);

  g_byte_array_ref(result_ptr);

  *_return = result_ptr;

  // printf("no_op() returning ");
  // print_n_bytes(result_ptr->data, result_ptr->len);

  free(int_arr);
  return TRUE;

}

/* TutorialSimpleArrCompHandler's instance finalizer (destructor) */
static void
tutorial_simple_arr_comp_handler_finalize (GObject *object)
{
  TutorialSimpleArrCompHandler *self =
    TUTORIAL_SIMPLE_ARR_COMP_HANDLER (object);

  /* Free our calculation-log hash table */
  g_hash_table_unref (self->log);
  self->log = NULL;

  /* Chain up to the parent class */
  G_OBJECT_CLASS (tutorial_simple_arr_comp_handler_parent_class)->
    finalize (object);
}

/* TutorialSimpleArrCompHandler's instance initializer (constructor) */
static void
tutorial_simple_arr_comp_handler_init (TutorialSimpleArrCompHandler *self)
{
  /* Create our calculation-log hash table */
  self->log = g_hash_table_new_full (g_int_hash,
                                     g_int_equal,
                                     g_free,
                                     g_object_unref);
}

/* TutorialSimpleArrCompHandler's class initializer */
static void
tutorial_simple_arr_comp_handler_class_init (TutorialSimpleArrCompHandlerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  SimpleArrCompHandlerClass *simple_arr_comp_handler_class =
    SIMPLE_ARR_COMP_HANDLER_CLASS (klass);

  /* Register our destructor */
  gobject_class->finalize = tutorial_simple_arr_comp_handler_finalize;

  /* Register our implementations of SimpleArrCompHandler's methods */
  simple_arr_comp_handler_class->increment_array = 
    tutorial_simple_arr_comp_handler_increment_array;
  simple_arr_comp_handler_class->add_arrays = 
    tutorial_simple_arr_comp_handler_add_arrays;
  simple_arr_comp_handler_class->mat_multiply = 
    tutorial_simple_arr_comp_handler_mat_multiply;
  simple_arr_comp_handler_class->word_count = 
    tutorial_simple_arr_comp_handler_word_count;
  simple_arr_comp_handler_class->sort_array = 
    tutorial_simple_arr_comp_handler_sort_array;
  simple_arr_comp_handler_class->no_op = 
    tutorial_simple_arr_comp_handler_no_op;
}

/* ---------------------------------------------------------------- */

/* That ends the implementation of TutorialSimpleArrCompHandler.
   Everything below is fairly generic code that sets up a minimal
   Thrift server for tutorial clients. */


/* Our server object, declared globally so it is accessible within the
   SIGINT signal handler */
ThriftServer *server = NULL;

/* A flag that indicates whether the server was interrupted with
   SIGINT (i.e. Ctrl-C) so we can tell whether its termination was
   abnormal */
gboolean sigint_received = FALSE;

/* Handle SIGINT ("Ctrl-C") signals by gracefully stopping the
   server */
static void
sigint_handler (int signal_number)
{
  THRIFT_UNUSED_VAR (signal_number);

  /* Take note we were called */
  sigint_received = TRUE;

  /* Shut down the server gracefully */
  if (server != NULL)
    thrift_server_stop (server);
}

int main (int argc, char *argv[])
{
  if (argc < 3) {
    printf("usage\n");
    return -1;
  }
  int c; 
  struct config myConf;
  while ((c = getopt (argc, argv, "c:")) != -1) { 
  switch (c) 
    { 
    case 'c':
      myConf = set_bb_config(optarg, 0);
      break;
    case '?': 
        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        printf("usage: -c config\n");
      return 1; 
    default:
      printf("usage\n");
      return -1;
    } 
  } 
  targetIP = init_sockets(&myConf, 0);
  set_host_list(myConf.hosts, myConf.num_hosts);

  TutorialSimpleArrCompHandler *handler;
  SimpleArrCompProcessor *processor;

  ThriftTransport *server_transport;
  ThriftTransportFactory *transport_factory;
  ThriftProtocolFactory *protocol_factory;

  struct sigaction sigint_action;

  GError *error = NULL;
  int exit_status = 0;

#if (!GLIB_CHECK_VERSION (2, 36, 0))
  g_type_init ();
#endif

  /* Create an instance of our handler, which provides the service's
     methods' implementation */
  handler =
    g_object_new (TYPE_TUTORIAL_SIMPLE_ARR_COMP_HANDLER,
                  NULL);

  /* Create an instance of the service's processor, automatically
     generated by the Thrift compiler, which parses incoming messages
     and dispatches them to the appropriate method in the handler */
  processor =
    g_object_new (TYPE_SIMPLE_ARR_COMP_PROCESSOR,
                  "handler", handler,
                  NULL);

  /* Create our server socket, which binds to the specified port and
     listens for client connections */
  server_transport =
    g_object_new (THRIFT_TYPE_UDP_SOCKET,
                  "listen_port", 9180,
                  "server", TRUE,
                  NULL);

  /* Create our transport factory, used by the server to wrap "raw"
     incoming connections from the client (in this case with a
     ThriftBufferedUDPTransport to improve performance) */
  transport_factory =
    g_object_new (THRIFT_TYPE_BUFFERED_UDP_TRANSPORT_FACTORY,
                  NULL);

  /* Create our protocol factory, which determines which wire protocol
     the server will use (in this case, Thrift's binary protocol) */
  protocol_factory =
    g_object_new (THRIFT_TYPE_BINARY_PROTOCOL_FACTORY,
                  NULL);

  /* Create the server itself */
  server =
    g_object_new (THRIFT_TYPE_SIMPLE_UDP_SERVER,
                  "processor",                processor,
                  "server_udp_transport",         server_transport,
                  "input_transport_factory",  transport_factory,
                  "output_transport_factory", transport_factory,
                  "input_protocol_factory",   protocol_factory,
                  "output_protocol_factory",  protocol_factory,
                  NULL);

  printf("Calling open on transport\n");
  // Open our socket so we can use it
  thrift_transport_open (server_transport, &error);
  printf("checking error\n");
  if (error != NULL) {
    printf ("ERROR: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }
  printf("memset\n");

  /* Install our SIGINT handler, which handles Ctrl-C being pressed by
     stopping the server gracefully (not strictly necessary, but a
     nice touch) */
  memset (&sigint_action, 0, sizeof (sigint_action));
  sigint_action.sa_handler = sigint_handler;
  sigint_action.sa_flags = SA_RESETHAND;
  sigaction (SIGINT, &sigint_action, NULL);

  /* Start the server, which will run until its stop method is invoked
     (from within the SIGINT handler, in this case) */
  printf ("Starting the server...\n");
  thrift_server_serve (server, &error);

  /* If the server stopped for any reason other than having been
     interrupted by the user, report the error */
  if (!sigint_received) {
    g_message ("thrift_server_serve: %s",
               error != NULL ? error->message : "(null)");
    g_clear_error (&error);
  }

  printf("done.\n");

  g_object_unref (server);
  g_object_unref (transport_factory);
  g_object_unref (protocol_factory);
  g_object_unref (server_transport);

  g_object_unref (processor);
  g_object_unref (handler);

  return exit_status;
}
