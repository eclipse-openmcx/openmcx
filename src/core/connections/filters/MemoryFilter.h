/********************************************************************************
 * Copyright (c) 2022 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_FILTERS_MEMORY_FILTER_H
#define MCX_CORE_CONNECTIONS_FILTERS_MEMORY_FILTER_H

#include "core/connections/filters/Filter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct MemoryFilter MemoryFilter;

typedef McxStatus (* fMemoryFilterSetup)(MemoryFilter * filter, ChannelType * type, size_t historySize, int reverseSearch);

extern const struct ObjectClass _MemoryFilter;

struct MemoryFilter {
    ChannelFilter _;

    fMemoryFilterSetup Setup;

    ChannelValue * valueHistoryRead;
    ChannelValue * valueHistoryWrite;
    double * timeHistoryRead;
    double * timeHistoryWrite;

    size_t numEntriesRead;
    size_t numEntriesWrite;
    size_t historySize;
} ;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_FILTERS_MEMORY_FILTER_H */