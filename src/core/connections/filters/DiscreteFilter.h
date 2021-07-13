/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_FILTERS_DISCRETEFILTER_H
#define MCX_CORE_CONNECTIONS_FILTERS_DISCRETEFILTER_H

#include "core/connections/filters/Filter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct DiscreteFilter DiscreteFilter;

typedef McxStatus (* fDiscreteFilterSetup)(DiscreteFilter * filter, ChannelType * type);

extern const struct ObjectClass _DiscreteFilter;

struct DiscreteFilter {
    ChannelFilter _; // base class

    fDiscreteFilterSetup Setup;

    ChannelValue lastCouplingStepValue;
    ChannelValue lastSynchronizationStepValue;

} ;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_FILTERS_DISCRETEFILTER_H */