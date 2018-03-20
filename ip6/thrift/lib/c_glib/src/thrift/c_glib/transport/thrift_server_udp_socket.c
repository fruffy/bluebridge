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

#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <thrift/c_glib/thrift.h>
#include <thrift/c_glib/transport/thrift_udp_socket.h>
#include <thrift/c_glib/transport/thrift_transport.h>
#include <thrift/c_glib/transport/thrift_server_transport.h>
#include <thrift/c_glib/transport/thrift_server_udp_socket.h>

/* object properties */
enum _ThriftServerUDPSocketProperties
{
  PROP_0,
  PROP_THRIFT_SERVER_UDP_SOCKET_PORT,
  PROP_THRIFT_SERVER_UDP_SOCKET_BACKLOG
};

/* define the GError domain string */
#define THRIFT_SERVER_UDP_SOCKET_ERROR_DOMAIN "thrift-server-udp-socket-error-quark"

/* for errors coming from socket() and connect() */
extern int errno;

G_DEFINE_TYPE(ThriftServerUDPSocket, thrift_server_udp_socket, THRIFT_TYPE_SERVER_TRANSPORT)

gboolean
thrift_server_udp_socket_listen (ThriftServerTransport *transport, GError **error)
{
  int enabled = 1; // for setsockopt()
#if defined(USE_IPV6)
  struct sockaddr_in6 pin;
  sa_family_t fam = AF_INET6;
#else
  struct sockaddr_in pin;
  sa_family_t fam = AF_INET;
#endif
  ThriftServerUDPSocket *tsocket = THRIFT_SERVER_UDP_SOCKET (transport);

  // Create a address structure 
  memset (&pin, 0, sizeof(pin));
#if defined(USE_IPV6)
  pin.sin6_family = fam;
  pin.sin6_addr = in6addr_any;
  pin.sin6_port = htons(tsocket->port);
#else
  pin.sin_family = fam;
  pin.sin_addr.s_addr = INADDR_ANY;
  pin.sin_port = htons(tsocket->port);
#endif

  // Create a socket
  if ((tsocket->sd = socket (fam, SOCK_DGRAM, 0)) == -1) {
    g_set_error (error, THRIFT_SERVER_UDP_SOCKET_ERROR,
                 THRIFT_SERVER_UDP_SOCKET_ERROR_SOCKET,
                 "failed to create socket - %s", strerror (errno));
    return FALSE;
  }

  // Set the address to reusable so you don't have to wait after restarting
  if (setsockopt(tsocket->sd, SOL_SOCKET, SO_REUSEADDR, &enabled,
                 sizeof(enabled)) == -1) {
    g_set_error (error, THRIFT_SERVER_UDP_SOCKET_ERROR,
                 THRIFT_SERVER_UDP_SOCKET_ERROR_SETSOCKOPT,
                 "unable to set SO_REUSEADDR - %s", strerror(errno));
    return FALSE;
  }

  // Bind to the socket
  if (bind(tsocket->sd, (struct sockaddr *) &pin, sizeof(pin)) == -1) {
    g_set_error (error, THRIFT_SERVER_UDP_SOCKET_ERROR,
                 THRIFT_SERVER_UDP_SOCKET_ERROR_BIND,
                 "failed to bind to port %d - %s",
                 tsocket->port, strerror(errno));
    return FALSE;
  }

  return TRUE;
}

ThriftTransport *
thrift_server_udp_socket_accept (ThriftServerTransport *transport, GError **error)
{
  int r;
  guint8 buf;
  guint8 resp;
  uint16_t port;
#if defined(USE_IPV6)
  struct sockaddr_in6 address;
  char addr[INET6_ADDRSTRLEN];
  sa_family_t fam = AF_INET6;
#else
  struct sockaddr_in address;
  char addr[INET_ADDRSTRLEN];
  sa_family_t fam = AF_INET;
#endif
  socklen_t addrlen = sizeof(address);
  ThriftUDPSocket *socket = NULL;

  ThriftServerUDPSocket *tsocket = THRIFT_SERVER_UDP_SOCKET (transport);

  // When we get a request, we spawn a new socket to handle that request. 
  // Peek to see the data and where it is from, when the socket is returned, the server
  // should read the message and address it. 
  r = recvfrom(tsocket->sd, &buf, 1, 0, (struct sockaddr *) &address, &addrlen);
  if (r == -1)
  {
    g_set_error (error, THRIFT_SERVER_UDP_SOCKET_ERROR,
                 THRIFT_SERVER_UDP_SOCKET_ERROR_ACCEPT,
                 "failed to receive connection request - %s",
                 strerror(errno));
    return FALSE;
  }

  // Convert both the address and port to host format (that's how open gets them)
#if defined(USE_IPV6)
  port = ntohs(address.sin6_port);
  inet_ntop(fam, &(address.sin6_addr), addr, INET6_ADDRSTRLEN);
#else
  port = ntohs(address.sin_port);
  inet_ntop(fam, &(address.sin_addr), addr, INET_ADDRSTRLEN);
#endif

  // Create new socket object to communicate with the client
  socket = g_object_new (THRIFT_TYPE_UDP_SOCKET, "hostname", addr, "port", port, "server", TRUE, NULL);

  // Open new socket
  if (!THRIFT_TRANSPORT_GET_CLASS(THRIFT_TRANSPORT(socket))->open(THRIFT_TRANSPORT(socket), error)) {
    g_message("thrift_server_udp_socket_accept: could not open socket");
    g_message("error: %s", (*error)->message);
    return FALSE;
  }

  resp = 5;
  // Send a response back to the client (giving them the new socket info)
  if (!THRIFT_TRANSPORT_GET_CLASS(THRIFT_TRANSPORT(socket))->write(THRIFT_TRANSPORT(socket), (const gpointer) &resp, 1, error)) {
    g_message("thrift_server_udp_socket_accept: could not send response");
    g_message("error: %s", (*error)->message);
    return FALSE;
  }

  return THRIFT_TRANSPORT(socket);
}

gboolean
thrift_server_udp_socket_close (ThriftServerTransport *transport, GError **error)
{
  ThriftServerUDPSocket *tsocket = THRIFT_SERVER_UDP_SOCKET (transport);

  if (close (tsocket->sd) == -1)
  {
    g_set_error (error, THRIFT_SERVER_UDP_SOCKET_ERROR,
                 THRIFT_SERVER_UDP_SOCKET_ERROR_CLOSE,
                 "unable to close socket - %s", strerror(errno));
    return FALSE;
  }
  tsocket->sd = THRIFT_INVALID_SOCKET;

  return TRUE;
}

/* define the GError domain for this implementation */
GQuark
thrift_server_udp_socket_error_quark (void)
{
  return g_quark_from_static_string(THRIFT_SERVER_UDP_SOCKET_ERROR_DOMAIN);
}

/* initializes the instance */
static void
thrift_server_udp_socket_init (ThriftServerUDPSocket *socket)
{
  socket->sd = THRIFT_INVALID_SOCKET;
}

/* destructor */
static void
thrift_server_udp_socket_finalize (GObject *object)
{
  ThriftServerUDPSocket *socket = THRIFT_SERVER_UDP_SOCKET (object);

  if (socket->sd != THRIFT_INVALID_SOCKET)
  {
    close (socket->sd);
  }
  socket->sd = THRIFT_INVALID_SOCKET;
}

/* property accessor */
void
thrift_server_udp_socket_get_property (GObject *object, guint property_id,
                                   GValue *value, GParamSpec *pspec)
{
  ThriftServerUDPSocket *socket = THRIFT_SERVER_UDP_SOCKET (object);

  switch (property_id)
  {
    case PROP_THRIFT_SERVER_UDP_SOCKET_PORT:
      g_value_set_uint (value, socket->port);
      break;
    case PROP_THRIFT_SERVER_UDP_SOCKET_BACKLOG:
      g_value_set_uint (value, socket->backlog);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/* property mutator */
void
thrift_server_udp_socket_set_property (GObject *object, guint property_id,
                                   const GValue *value, GParamSpec *pspec)
{
  ThriftServerUDPSocket *socket = THRIFT_SERVER_UDP_SOCKET (object);

  switch (property_id)
  {
    case PROP_THRIFT_SERVER_UDP_SOCKET_PORT:
      socket->port = g_value_get_uint (value);
      break;
    case PROP_THRIFT_SERVER_UDP_SOCKET_BACKLOG:
      socket->backlog = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/* initializes the class */
static void
thrift_server_udp_socket_class_init (ThriftServerUDPSocketClass *cls)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec = NULL;

  /* setup accessors and mutators */
  gobject_class->get_property = thrift_server_udp_socket_get_property;
  gobject_class->set_property = thrift_server_udp_socket_set_property;

  param_spec = g_param_spec_uint ("port",
                                  "port (construct)",
                                  "Set the port to listen to",
                                  0, /* min */
                                  65534, /* max */
                                  9090, /* default by convention */
                                  G_PARAM_CONSTRUCT_ONLY |
                                  G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_THRIFT_SERVER_UDP_SOCKET_PORT, 
                                   param_spec);

  param_spec = g_param_spec_uint ("backlog",
                                  "backlog (construct)",
                                  "Set the accept backlog",
                                  0, /* max */
                                  65534, /* max */
                                  1024, /* default */
                                  G_PARAM_CONSTRUCT_ONLY |
                                  G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_THRIFT_SERVER_UDP_SOCKET_BACKLOG,
                                   param_spec);

  gobject_class->finalize = thrift_server_udp_socket_finalize;

  ThriftServerTransportClass *tstc = THRIFT_SERVER_TRANSPORT_CLASS (cls);
  tstc->listen = thrift_server_udp_socket_listen;
  tstc->accept = thrift_server_udp_socket_accept;
  tstc->close = thrift_server_udp_socket_close;
}

