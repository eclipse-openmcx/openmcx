/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_FILTERS_INT_EXT_FILTER_H
#define MCX_CORE_CONNECTIONS_FILTERS_INT_EXT_FILTER_H

#include "core/connections/filters/Filter.h"
#include "core/connections/filters/IntFilter.h"
#include "core/connections/filters/ExtFilter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct IntExtFilter IntExtFilter;

typedef McxStatus (* fIntExtFilterSetup)(IntExtFilter * intExtFilter, int degreeExtrapolation, int degreeInterpolation, size_t buffSize);

extern const struct ObjectClass _IntExtFilter;

struct IntExtFilter {
    ChannelFilter _;

    fIntExtFilterSetup Setup;

    IntFilter * filterInt;
    ExtFilter * filterExt;
};


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_FILTERS_INT_EXT_FILTER_H */