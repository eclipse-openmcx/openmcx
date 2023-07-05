/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_FILTERS_FILTER_H
#define MCX_CORE_CONNECTIONS_FILTERS_FILTER_H

#include "core/connections/Connection.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ChannelFilter ChannelFilter;

typedef McxStatus (* fChannelFilterSetValue)(ChannelFilter * filter, double time, ChannelValueData value);
typedef ChannelValueData (* fChannelFilterGetValue)(ChannelFilter * filter, double time);

typedef McxStatus (* fChannelFilterEnterCouplingStepMode)(ChannelFilter * filter
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize);
typedef McxStatus (* fChannelFilterEnterInitializationMode)(ChannelFilter * filter);
typedef McxStatus (* fChannelFilterEnterCommunicationMode)(ChannelFilter * filter, double time);

typedef McxStatus (* fChannelFilterAssignState)(ChannelFilter * filter, ConnectionState * state);
extern const struct ObjectClass _ChannelFilter;

struct ChannelFilter {
    Object _; // base

    ConnectionState * state;

    // custom data is specified in child class

    fChannelFilterSetValue SetValue;
    fChannelFilterGetValue GetValue;

    fChannelFilterEnterInitializationMode EnterInitializationMode;
    fChannelFilterEnterCouplingStepMode EnterCouplingStepMode;
    fChannelFilterEnterCommunicationMode EnterCommunicationMode;

    fChannelFilterAssignState AssignState;
};

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_FILTERS_FILTER_H */