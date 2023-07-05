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
#include "core/connections/filters/Filter.h"
#include "core/connections/filters/IntExtFilter.h"
#include "util/compare.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus IntExtFilterSetValue(ChannelFilter * filter, double time, ChannelValueData _value) {
    IntExtFilter * intExtFilter = (IntExtFilter *)filter;
    ChannelFilter * filterInt = (ChannelFilter *)intExtFilter->filterInt;
    ChannelFilter * filterExt = (ChannelFilter *)intExtFilter->filterExt;

    if (RETURN_ERROR == filterInt->SetValue(filterInt, time, _value)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Set value of interpolation filter failed");
        return RETURN_ERROR;
    }

    if (RETURN_ERROR == filterExt->SetValue(filterExt, time, _value)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Set value of extrapolation filter failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static ChannelValueData IntExtFilterGetValue(ChannelFilter * filter, double time) {
    IntExtFilter * intExtFilter = (IntExtFilter *)filter;
    ChannelFilter * filterInt = (ChannelFilter *) intExtFilter->filterInt;
    ChannelFilter * filterExt = (ChannelFilter *) intExtFilter->filterExt;

    ChannelValueData value;

    // time out of interpolation data -> try extrapolate
    if (intExtFilter->filterInt->nReadCouplingSteps == 0 ||
        double_lt(time, intExtFilter->filterInt->read_x_data[0]) ||
        double_gt(time, intExtFilter->filterInt->read_x_data[intExtFilter->filterInt->nReadCouplingSteps - 1]))
    {
        // time within extrapolation data -> extrapolate with interpolation data
        if (0 == mcx_poly_get_n(intExtFilter->filterExt->polyStruct)
            || (double_lt(time, mcx_poly_get_x(intExtFilter->filterExt->polyStruct, mcx_poly_get_n(intExtFilter->filterExt->polyStruct) - 1)) &&
                double_gt(time, mcx_poly_get_x(intExtFilter->filterExt->polyStruct, 0)))) {
            mcx_log(LOG_WARNING, "Connection: IntExtFilter: Out of bounds for interpolation and extrapolation, extrapolate from interpolation data");
            value = filterInt->GetValue(filterInt, time);
        }
        // Extrapolation
        else {
            value = filterExt->GetValue(filterExt, time);
        }
    }
    // Interpolation
    else {
        value = filterInt->GetValue(filterInt, time);
    }

    return value;
}

static McxStatus IntExtFilterEnterCouplingStepMode(ChannelFilter * filter, double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize) {
    IntExtFilter * intExtFilter = (IntExtFilter *)filter;
    ChannelFilter * filterInt = (ChannelFilter *) intExtFilter->filterInt;
    ChannelFilter * filterExt = (ChannelFilter *) intExtFilter->filterExt;

    if (RETURN_ERROR == filterInt->EnterCouplingStepMode(filterInt, communicationTimeStepSize, sourceTimeStepSize, targetTimeStepSize)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Enter coupling step mode of interpolation filter failed");
        return RETURN_ERROR;
    }

    if (RETURN_ERROR == filterExt->EnterCouplingStepMode(filterExt, communicationTimeStepSize, sourceTimeStepSize, targetTimeStepSize)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Enter coupling step mode of extrapolation filter failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus IntExtFilterEnterCommunicationMode(ChannelFilter * filter, double _time) {
    IntExtFilter * intExtFilter = (IntExtFilter *) filter;
    ChannelFilter * filterInt = (ChannelFilter *) intExtFilter->filterInt;
    ChannelFilter * filterExt = (ChannelFilter *) intExtFilter->filterExt;

    if (RETURN_ERROR == filterInt->EnterCommunicationMode(filterInt, _time)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Enter communication mode of interpolation filter failed");
        return RETURN_ERROR;
    }

    if (RETURN_ERROR == filterExt->EnterCommunicationMode(filterExt, _time)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Enter communication mode of extrapolation filter failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus IntExtFilterSetup(IntExtFilter * intExtFilter, int degreeExtrapolation, int degreeInterpolation, size_t buffSize) {
    if (RETURN_ERROR == intExtFilter->filterInt->Setup(intExtFilter->filterInt, degreeInterpolation, buffSize)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Setup of interpolation filter failed");
        return RETURN_ERROR;
    }

    if (RETURN_ERROR == intExtFilter->filterExt->Setup(intExtFilter->filterExt, degreeExtrapolation)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Setup of extrapolation filter failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus IntExtFilterEnterInitializationMode(ChannelFilter * filter) {
    IntExtFilter * intExtFilter = (IntExtFilter *)filter;
    ChannelFilter * filterInt = (ChannelFilter *)intExtFilter->filterInt;
    ChannelFilter * filterExt = (ChannelFilter *)intExtFilter->filterExt;
    if (RETURN_ERROR == filterInt->EnterInitializationMode(filterInt)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Assign state of interpolation filter failed");
        return RETURN_ERROR;
    }

    if (RETURN_ERROR == filterExt->EnterInitializationMode(filterExt)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Assign state of extrapolation filter failed");
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static McxStatus IntExtFilterAssignState(ChannelFilter * filter, ConnectionState * state) {
    IntExtFilter * intExtFilter = (IntExtFilter *)filter;
    ChannelFilter * filterInt = (ChannelFilter *)intExtFilter->filterInt;
    ChannelFilter * filterExt = (ChannelFilter *)intExtFilter->filterExt;
    if (RETURN_ERROR == filterInt->AssignState(filterInt, state)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Assign state of interpolation filter failed");
        return RETURN_ERROR;
    }

    if (RETURN_ERROR == filterExt->AssignState(filterExt, state)) {
        mcx_log(LOG_ERROR, "Connection: IntExtFilter: Assign state of extrapolation filter failed");
        return RETURN_ERROR;
    }

    filter->state = state;
    return RETURN_OK;
}

static void IntExtFilterDestructor(IntExtFilter * intExtFilter) {
    object_destroy(intExtFilter->filterInt);
    object_destroy(intExtFilter->filterExt);
}

static IntExtFilter * IntExtFilterCreate(IntExtFilter * intExtFilter) {
    ChannelFilter * filter = (ChannelFilter *) intExtFilter;

    filter->GetValue = IntExtFilterGetValue;
    filter->SetValue = IntExtFilterSetValue;

    filter->EnterCommunicationMode  = IntExtFilterEnterCommunicationMode;
    filter->EnterCouplingStepMode      = IntExtFilterEnterCouplingStepMode;
    filter->EnterInitializationMode = IntExtFilterEnterInitializationMode;

    filter->AssignState = IntExtFilterAssignState;

    intExtFilter->Setup = IntExtFilterSetup;

    intExtFilter->filterInt = (IntFilter *) object_create(IntFilter);
    intExtFilter->filterExt = (ExtFilter *) object_create(ExtFilter);

    return intExtFilter;
}

OBJECT_CLASS(IntExtFilter, ChannelFilter);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */