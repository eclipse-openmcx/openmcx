/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/channels/ChannelDimension.h"
#include "core/channels/ConnectionStatus.h"
#include "util/stdlib.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

ConnectionStatus * CreateConnectionStatus(ChannelDimension * dimension) {
    size_t i;
    ConnectionStatus * connectionStatusNew = (ConnectionStatus *) mcx_calloc(1, sizeof(ConnectionStatus));
    if (!connectionStatusNew) {
        return NULL;
    }

    if (!dimension) {
        connectionStatusNew->num = 0;
        connectionStatusNew->startIdxs = NULL;
        connectionStatusNew->endIdxs = NULL;
        connectionStatusNew->connected.scalar = 0;
        return connectionStatusNew;
    }

    connectionStatusNew->num = dimension->num;

    connectionStatusNew->startIdxs = (size_t *) mcx_calloc(connectionStatusNew->num, sizeof(size_t));
    if (!connectionStatusNew->startIdxs) {
        goto error_cleanup;
    }

    connectionStatusNew->endIdxs = (size_t *) mcx_calloc(connectionStatusNew->num, sizeof(size_t));
    if (!connectionStatusNew->endIdxs) {
        goto error_cleanup;
    }

    memcpy(connectionStatusNew->startIdxs, dimension->startIdxs, connectionStatusNew->num * sizeof(size_t));
    memcpy(connectionStatusNew->endIdxs, dimension->endIdxs, connectionStatusNew->num * sizeof(size_t));

    connectionStatusNew->connected.array = (int **) mcx_calloc(connectionStatusNew->num, sizeof(int *));
    if (!connectionStatusNew->connected.array) {
        goto error_cleanup;
    }

    for (i = 0; i < connectionStatusNew->num; i++) {
        connectionStatusNew->connected.array[i] = (int *)
            mcx_calloc(connectionStatusNew->endIdxs[i] - connectionStatusNew->startIdxs[i] + 1, sizeof(int));
        if (!connectionStatusNew->connected.array[i]) {
            goto error_cleanup;
        }
    }
    return connectionStatusNew;

error_cleanup:
    if (connectionStatusNew->startIdxs) {
        mcx_free(connectionStatusNew->startIdxs);
    }
    if (connectionStatusNew->endIdxs) {
        mcx_free(connectionStatusNew->endIdxs);
    }
    if (connectionStatusNew->num > 0 && connectionStatusNew->connected.array) {
        for (i = 0; i < connectionStatusNew->num; i++) {
            if (connectionStatusNew->connected.array[i]) {
                mcx_free(connectionStatusNew->connected.array[i]);
            }
        }
        mcx_free(connectionStatusNew->connected.array);
    }

    if (connectionStatusNew) {
        mcx_free(connectionStatusNew);
    }
    return NULL;
}

void DestroyConnectionStatus(ConnectionStatus * connectionStatus) {
    size_t i;
    for (i = 0; i < connectionStatus->num; i++) {
        if (connectionStatus->connected.array[i]) {
            mcx_free(connectionStatus->connected.array[i]);
        }
    }

    if (connectionStatus->num > 0 && connectionStatus->connected.array) {
        mcx_free(connectionStatus->connected.array);
    }

    if (connectionStatus->endIdxs) {
        mcx_free(connectionStatus->endIdxs);
    }
    if (connectionStatus->startIdxs) {
        mcx_free(connectionStatus->startIdxs);
    }
    if (connectionStatus) {
        mcx_free(connectionStatus);
    }

}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */