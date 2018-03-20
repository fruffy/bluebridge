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

#ifndef _THRIFT_UDP_SOCKET_H
#define _THRIFT_UDP_SOCKET_H

#include <glib-object.h>
#include <sys/socket.h>

#include <thrift/c_glib/transport/thrift_transport.h>

G_BEGIN_DECLS

#define USE_IPV6

/*! \file thrift_udp_socket.h
 *  \brief UDP socket implementation of a Thrift transport.  Subclasses the
 *         ThriftTransport class.
 */

/* type macros */
#define THRIFT_TYPE_UDP_SOCKET (thrift_udp_socket_get_type ())
#define THRIFT_UDP_SOCKET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THRIFT_TYPE_UDP_SOCKET, ThriftUDPSocket))
#define THRIFT_IS_UDP_SOCKET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THRIFT_TYPE_UDP_SOCKET))
#define THRIFT_UDP_SOCKET_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), THRIFT_TYPE_UDP_SOCKET, ThriftUDPSocketClass))
#define THRIFT_IS_UDP_SOCKET_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), THRIFT_TYPE_UDP_SOCKET))
#define THRIFT_UDP_SOCKET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THRIFT_TYPE_UDP_SOCKET, ThriftUDPSocketClass))

typedef struct _ThriftUDPSocket ThriftUDPSocket;

/*!
 * Thrift Socket instance.
 */
struct _ThriftUDPSocket
{
  ThriftTransport parent;

  /* private */
  gchar *hostname;                // String hostname of the machine we "connect" to (TCP legacy)
#if defined(USE_IPV6)
  struct sockaddr_in6 *conn_sock; // The socket we should be sending to
#else
  struct sockaddr_in *conn_sock;  // The socket we should be sending to
#endif
  socklen_t conn_size;            // The size of our sockaddr connection (either sizeof(sockaddr_in) or sizeof(sockaddr_in6))
  gshort port;                    // The port this socket was initialized to connect to (TCP legacy)
  gboolean server;                // Determines if this socket was created by a client or server
  int sd;                         // The socket descriptor
  guint8 *buf;                    // Unused
  guint32 buf_size;               // Unused
};

typedef struct _ThriftUDPSocketClass ThriftUDPSocketClass;

/*!
 * Thrift Socket class.
 */
struct _ThriftUDPSocketClass
{
  ThriftTransportClass parent;
};

/* used by THRIFT_TYPE_UDP_SOCKET */
GType thrift_udp_socket_get_type (void);

G_END_DECLS

#endif
