/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CONNECTION_STATUS_H
#define MCX_CORE_CHANNELS_CONNECTION_STATUS_H

#include "CentralParts.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



typedef struct ConnectionStatus {
    size_t num;
    size_t * startIdxs;
    size_t * endIdxs;

    union {
        int scalar;
        int ** array;
    } connected;
} ConnectionStatus;

ConnectionStatus * CreateConnectionStatus(ChannelDimension * dimension);
void DestroyConnectionStatus(ConnectionStatus * connectionStatus);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_CORE_CHANNELS_CONNECTION_STATUS_H
#pragma once