/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"

#if defined (ENABLE_MT)

#include "util/socket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int mcx_socket_set_timeout(McxSocket sockfd, int msTimeout) {
    struct timeval tv;
    tv.tv_sec = msTimeout / 1000; // 1s == 1000ms
    tv.tv_usec = (msTimeout % 1000) * 1000; // 1ms == 1000us
    if (tv.tv_usec >= 1000000) { // 1s == 1000000us
    tv.tv_sec += 1;
    tv.tv_usec -= 1000000;
    }

    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
}

int mcx_socket_set_nodelay(McxSocket sockfd, int noDelay) {
    return setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //ENABLE_MT