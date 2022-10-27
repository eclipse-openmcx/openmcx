/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_FILTEREDCONNECTION_H
#define MCX_CORE_CONNECTIONS_FILTEREDCONNECTION_H

#include "core/connections/Connection.h"
#include "core/Conversion.h"
#include "core/channels/ChannelValue.h"
#include "core/connections/filters/Filter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct FilteredConnectionData {

    // storage of the filtered value provided by the output channel
    ChannelValue store;

    // storage for temporary results during out channel updates
    ChannelValue updateBuffer;

    ChannelFilter ** filters;
    ChannelFilter * _filter;
    size_t numFilters;

} FilteredConnectionData;

typedef struct FilteredConnection FilteredConnection;

typedef struct ChannelFilter * (* fConnectionGetFilter)(FilteredConnection * connection, size_t idx);

typedef McxStatus (* fFilteredConnectionSetResult)(FilteredConnection * connection, const void * value);

extern const struct ObjectClass _FilteredConnection;

struct FilteredConnection {
    Connection _; /* super class first */

    fConnectionGetFilter GetReadFilter;
    fConnectionGetFilter GetWriteFilter;

    fFilteredConnectionSetResult SetResult;

    FilteredConnectionData data;
} ;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_FILTEREDCONNECTION_H */