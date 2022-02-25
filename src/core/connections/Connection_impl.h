/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_CONNECTION_IMPL_H
#define MCX_CORE_CONNECTIONS_CONNECTION_IMPL_H

// for ExtrapolType
#include "core/connections/filters/Filter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Connection

// provides information about the connection between
// the input channel it is stored in and the output channel
// that the input channel is connected to

extern const struct ObjectClass _ConnectionData;

typedef struct ConnectionData {
    Object _; // base class

    // ----------------------------------------------------------------------
    // Structural Information

    // source
    struct ChannelOut * out;

    // target
    struct ChannelIn * in;


    // ----------------------------------------------------------------------
    // Value on channel

    const void * value;
    int useInitialValue;

    ChannelValue store;

    int isActiveDependency;

    // Meta Data
    ConnectionInfo info;

    // Current state of the connection in state machine
    ConnectionState state;

    // Temporary save functions during initialization mode
    fConnectionUpdateFromInput NormalUpdateFrom;
    fConnectionUpdateToOutput NormalUpdateTo;
    const void * normalValue;
} ConnectionData;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_CONNECTION_IMPL_H */