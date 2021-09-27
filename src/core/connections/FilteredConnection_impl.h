/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_FILTEREDCONNECTIONDATA_H
#define MCX_CORE_CONNECTIONS_FILTEREDCONNECTIONDATA_H

#include "core/Conversion.h"

// for ExtrapolType
#include "core/connections/filters/Filter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const struct ObjectClass _FilteredConnectionData;

typedef struct FilteredConnectionData {
    Object _; /* super class */

    // storage of the filtered value provided by the output channel
    ChannelValue store;

    ChannelFilter ** filters;
    size_t numFilters;

} FilteredConnectionData;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_FILTEREDCONNECTIONDATA_H */