/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CUTILS_SOCKETS_H
#define __CUTILS_SOCKETS_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef HAVE_WINSOCK
#include <winsock2.h>
typedef int  socklen_t;
#else
#include <sys/socket.h>
#endif

#define ANDROID_SOCKET_DIR		"/run"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * See also android.os.LocalSocketAddress.Namespace
 */
// Linux "abstract" (non-filesystem) namespace
#define ANDROID_SOCKET_NAMESPACE_ABSTRACT 0
// Android "reserved" (/dev/socket) namespace
#define ANDROID_SOCKET_NAMESPACE_RESERVED 1
// Normal filesystem namespace
#define ANDROID_SOCKET_NAMESPACE_FILESYSTEM 2

extern int socket_local_server(const char *name, int namespaceId, int type);
extern int socket_local_server_bind(int s, const char *name, int namespaceId);
extern int socket_local_client_connect(int fd, 
        const char *name, int namespaceId, int type);
extern int socket_local_client(const char *name, int namespaceId, int type);


#ifdef __cplusplus
}
#endif

#endif /* __CUTILS_SOCKETS_H */ 
