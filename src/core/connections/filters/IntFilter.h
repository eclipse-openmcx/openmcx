/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_FILTERS_INT_FILTER_H
#define MCX_CORE_CONNECTIONS_FILTERS_INT_FILTER_H

#include "core/connections/filters/Filter.h"

#include "core/Interpolation.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct IntFilter IntFilter;

typedef McxStatus (* fIntFilterSetup)(IntFilter * filter, int degree, size_t buffSize);

extern const struct ObjectClass _IntFilter;

struct IntFilter {
    ChannelFilter _;

    fIntFilterSetup Setup;
    int couplingPolyDegree;

    mcx_table* T;

    double lastCouplingStepTime;

    size_t dataLen;

    size_t nReadCouplingSteps;
    double * read_x_data;
    double * read_y_data;

    size_t nWriteCouplingSteps;
    double * write_x_data;
    double * write_y_data;
};


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_FILTERS_INT_FILTER_H */