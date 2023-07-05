/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"
#include "channel_value/ChannelValue.h"
#include "core/connections/filters/Filter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Generic ChannelFilter

McxStatus ChannelFilterSetValue(ChannelFilter * filter, double time, ChannelValueData value) {
    return RETURN_OK;
}

ChannelValueData ChannelFilterGetValue(ChannelFilter * filter, double time) {
    ChannelValueData value = { 0.0 };
    return value;
}

static McxStatus ChannelFilterEnterInitializationMode(ChannelFilter * filter) {
    return RETURN_OK;
}

static McxStatus ChannelFilterEnterCouplingStepMode(ChannelFilter * filter
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize)
{
    return RETURN_OK;
}

static McxStatus ChannelFilterEnterCommunicationMode(ChannelFilter * filter, double time) {
    return RETURN_OK;
}


static McxStatus ChannelFilterAssignState(ChannelFilter * filter, ConnectionState * state) {
    filter->state = state;
    return RETURN_OK;
}


static void ChannelFilterDestructor(ChannelFilter * filter) {

}

static ChannelFilter * ChannelFilterCreate(ChannelFilter * filter) {
    filter->SetValue = ChannelFilterSetValue;
    filter->GetValue = ChannelFilterGetValue;

    filter->EnterInitializationMode = ChannelFilterEnterInitializationMode;
    filter->EnterCouplingStepMode = ChannelFilterEnterCouplingStepMode;
    filter->EnterCommunicationMode = ChannelFilterEnterCommunicationMode;

    filter->AssignState = ChannelFilterAssignState;
    return filter;
}

OBJECT_CLASS(ChannelFilter, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */