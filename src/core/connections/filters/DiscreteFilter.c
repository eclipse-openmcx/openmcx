/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/connections/filters/DiscreteFilter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static McxStatus DiscreteFilterSetValue(ChannelFilter * filter, double time, ChannelValueData value) {
    DiscreteFilter * discreteFilter = (DiscreteFilter *) filter;

    if (InCommunicationMode != * filter->state) {
        if (RETURN_OK != ChannelValueSetFromReference(&discreteFilter->lastCouplingStepValue, &value)) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static ChannelValueData DiscreteFilterGetValue(ChannelFilter * filter, double time) {
    DiscreteFilter * discreteFilter = (DiscreteFilter *) filter;

    return * (ChannelValueData *) ChannelValueDataPointer(&discreteFilter->lastSynchronizationStepValue);
}

static McxStatus DiscreteFilterEnterCouplingStepMode(ChannelFilter * filter
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize)
{
    return RETURN_OK;
}

static McxStatus DiscreteFilterEnterCommunicationMode(ChannelFilter * filter, double _time) {
    DiscreteFilter * discreteFilter = (DiscreteFilter *) filter;

    ChannelValueSet(&discreteFilter->lastSynchronizationStepValue, &discreteFilter->lastCouplingStepValue);

    return RETURN_OK;
}

static McxStatus DiscreteFilterSetup(DiscreteFilter * filter, const ChannelType * type) {
    ChannelValueInit(&filter->lastSynchronizationStepValue, ChannelTypeClone(type));
    ChannelValueInit(&filter->lastCouplingStepValue, ChannelTypeClone(type));

    return RETURN_OK;
}

static void DiscreteFilterDestructor(DiscreteFilter * filter) {
    ChannelValueDestructor(&filter->lastSynchronizationStepValue);
    ChannelValueDestructor(&filter->lastCouplingStepValue);
}

static DiscreteFilter * DiscreteFilterCreate(DiscreteFilter * discreteFilter) {
    ChannelFilter * filter = (ChannelFilter *) discreteFilter;

    filter->SetValue = DiscreteFilterSetValue;
    filter->GetValue = DiscreteFilterGetValue;

    filter->EnterCommunicationMode = DiscreteFilterEnterCommunicationMode;
    filter->EnterCouplingStepMode = DiscreteFilterEnterCouplingStepMode;

    discreteFilter->Setup = DiscreteFilterSetup;

    ChannelValueInit(&discreteFilter->lastSynchronizationStepValue, ChannelTypeClone(&ChannelTypeUnknown));
    ChannelValueInit(&discreteFilter->lastCouplingStepValue, ChannelTypeClone(&ChannelTypeUnknown));

    return discreteFilter;
}

OBJECT_CLASS(DiscreteFilter, ChannelFilter);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */