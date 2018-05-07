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
#include <ctype.h>

#include <thrift/c_glib/thrift.h>
#include <thrift/c_glib/protocol/thrift_binary_protocol_factory.h>
#include <thrift/c_glib/protocol/thrift_protocol_factory.h>
#include <thrift/c_glib/server/thrift_server.h>
#include <thrift/c_glib/server/thrift_simple_server.h>
#include <thrift/c_glib/transport/thrift_buffered_transport_factory.h>
#include <thrift/c_glib/transport/thrift_server_socket.h>
#include <thrift/c_glib/transport/thrift_server_transport.h>

#include "gen-c_glib/simple_arr_comp.h"
#include "lib/client_lib.h"
#include "lib/config.h"
#include "lib/utils.h"

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

/* ---------------------------------------------------------------- */

/* The implementation of TutorialSimpleArrCompHandler follows. */

G_DEFINE_TYPE (TutorialSimpleArrCompHandler,
               tutorial_simple_arr_comp_handler,
               TYPE_SIMPLE_ARR_COMP_HANDLER);

static gboolean
tutorial_simple_arr_comp_handler_increment_array (SimpleArrCompIf *iface,
                                                           GByteArray                  **_return,
                                                           const GByteArray             *arr,
                                                           const gint8               value,
                                                           const gint32              length,
                                                           CallException           **ouch,
                                                           GError                  **error) 
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);

  //printf("Increment array\n");

  *_return = g_byte_array_sized_new(length);
  // result_arr = g_array_append_vals(result_arr, arr, length);

  // Increment the values
  for (int i = 0; i < length; i++) {
    gint8 val = g_array_index(arr, gint8, i) + value;
    g_byte_array_append(*_return, &val, sizeof(gint8));
  }
  // *_return = result_arr;
  return TRUE;
}

static gboolean
tutorial_simple_arr_comp_handler_add_arrays (SimpleArrCompIf *iface,
                                                      GByteArray                  **_return,
                                                      const GByteArray             *array1,
                                                      const GByteArray             *array2,
                                                      const gint32              length,
                                                      CallException           **ouch,
                                                      GError                  **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);

  //printf("Add arrays\n");

  *_return = g_byte_array_new();

  for (int i = 0; i < length; i++) {
    gint8 val = g_array_index(array1, gint8, i) + g_array_index(array2, gint8, i);
    g_byte_array_append(*_return, &val, sizeof(gint8));
  }

  // *_return = result_array;
  return TRUE;
}

static gint8 dot_prod_helper(GArray* x, const GArray* y, int m) {
  gint8 res = 0;

  for (int i = 0; i < m; i++) {
    res += g_array_index(x, gint8, i) + g_array_index(y, gint8, i);
  }

  return res;
}

static gboolean
tutorial_simple_arr_comp_handler_mat_multiply (SimpleArrCompIf *iface,
                                                        GArray                  **_return,
                                                        const GArray             *array,
                                                        const GPtrArray          *matrix,
                                                        const gint32              length,
                                                        const tuple              *dimension,
                                                        CallException           **ouch,
                                                        GError                  **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);

  //printf("Matrix multiply\n");

  if (length != dimension->m) {

    ouch = g_object_new(TYPE_CALL_EXCEPTION,
                        "err_code", 3,
                        "message", g_strdup_printf("Vector length (%d) does not match matrix columns (%d)", length, dimension->m),
                        NULL);
    return FALSE;
  } else {
    THRIFT_UNUSED_VAR (ouch);
  }

  GArray* result_array = g_array_new(FALSE, FALSE, sizeof(gint8));

  for (int i = 0; i < dimension->n; i++) {
    GArray* row = g_ptr_array_index(matrix, i);
    gint8 value = dot_prod_helper(row, array, dimension->m);
    g_array_append_val(result_array, value);
  }

  *_return = result_array;

  return TRUE;

}

static gboolean
tutorial_simple_arr_comp_handler_word_count (SimpleArrCompIf *iface,
                                                      gint32                   *_return,
                                                      const gchar              *story,
                                                      const gint32              length,
                                                      CallException           **ouch,
                                                      GError                  **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);
  gint32 wordcount = 0;
  char prev = ' '; // Start out with blank prev char.

  for (int i = 0; i < length; i++) {
    if (isspace(story[i]) && isgraph(prev)) {
      wordcount++;
    }
    prev = story[i];
  } 

  *_return = wordcount;

  //printf("word_count (%d): ", length);
  // print_n_bytes(story->data, story->len);

  return TRUE;

}

static int my_int_sort_function (gconstpointer a, gconstpointer b)
{
    int int_a = GPOINTER_TO_INT (a);
    int int_b = GPOINTER_TO_INT (b);

    return int_a - int_b;
}

static gboolean
tutorial_simple_arr_comp_handler_sort_array (SimpleArrCompIf *iface,
                                                      GArray                  **_return,
                                                      const GArray             *num_array,
                                                      const gint32              length,
                                                      CallException           **ouch,
                                                      GError                  **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);
  THRIFT_UNUSED_VAR (ouch);

  GArray* res = g_array_sized_new(FALSE, TRUE, sizeof(gint8), length);
  res = g_array_append_vals(res, num_array->data, length);
  g_array_sort(res, my_int_sort_function);

  *_return = res;

  // printf("sort_array(%d): ", length);
  // print_n_bytes(num_array->data, num_array->len);

  return TRUE;

}

static gboolean
tutorial_simple_arr_comp_handler_no_op (SimpleArrCompIf  *iface,
                                                 GByteArray                   **_return,
                                                 const GByteArray              *num_array,
                                                 const gint32               length,
                                                 GError                   **error)
{
  THRIFT_UNUSED_VAR (iface);
  THRIFT_UNUSED_VAR (error);

  GByteArray* result_array = g_byte_array_new();

  g_byte_array_append(result_array, num_array->data, length);

  *_return = result_array;

  // printf("no_op() returning ");
  // print_n_bytes(result_ptr->data, result_ptr->len);

  // free(int_arr);
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
  THRIFT_UNUSED_VAR(argv);

  if (argc > 1) {
    printf("usage\n");
    return -1;
  }
  TutorialSimpleArrCompHandler *handler;
  SimpleArrCompProcessor *processor;

  ThriftServerTransport *server_transport;
  ThriftTransportFactory *transport_factory;
  ThriftProtocolFactory *protocol_factory;

  struct sigaction sigint_action;

  GError *error;
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
    g_object_new (THRIFT_TYPE_SERVER_SOCKET,
                  "port", 9280,
                  NULL);

  /* Create our transport factory, used by the server to wrap "raw"
     incoming connections from the client (in this case with a
     ThriftBufferedUDPTransport to improve performance) */
  transport_factory =
    g_object_new (THRIFT_TYPE_BUFFERED_TRANSPORT_FACTORY,
                  NULL);

  /* Create our protocol factory, which determines which wire protocol
     the server will use (in this case, Thrift's binary protocol) */
  protocol_factory =
    g_object_new (THRIFT_TYPE_BINARY_PROTOCOL_FACTORY,
                  NULL);

  /* Create the server itself */
  server =
    g_object_new (THRIFT_TYPE_SIMPLE_SERVER,
                  "processor",                processor,
                  "server_transport",         server_transport,
                  "input_transport_factory",  transport_factory,
                  "output_transport_factory", transport_factory,
                  "input_protocol_factory",   protocol_factory,
                  "output_protocol_factory",  protocol_factory,
                  NULL);

  /* Install our SIGINT handler, which handles Ctrl-C being pressed by
     stopping the server gracefully (not strictly necessary, but a
     nice touch) */
  memset (&sigint_action, 0, sizeof (sigint_action));
  sigint_action.sa_handler = sigint_handler;
  sigint_action.sa_flags = SA_RESETHAND;
  sigaction (SIGINT, &sigint_action, NULL);

  /* Start the server, which will run until its stop method is invoked
     (from within the SIGINT handler, in this case) */
  puts ("Starting the server...");
  thrift_server_serve (server, &error);

  /* If the server stopped for any reason other than having been
     interrupted by the user, report the error */
  if (!sigint_received) {
    g_message ("thrift_server_serve: %s",
               error != NULL ? error->message : "(null)");
    g_clear_error (&error);
  }

  puts ("done.");

  g_object_unref (server);
  g_object_unref (transport_factory);
  g_object_unref (protocol_factory);
  g_object_unref (server_transport);

  g_object_unref (processor);
  g_object_unref (handler);

  return exit_status;
}
