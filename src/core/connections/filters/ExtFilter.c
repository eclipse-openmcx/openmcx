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
#include "core/connections/filters/ExtFilter.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus ExtFilterSetValue(ChannelFilter * filter, double time, ChannelValueData _value) {
    ExtFilter * extFilter = (ExtFilter *) filter;
    double value = _value.d;

    if (* filter->state != InCommunicationMode) {
        extFilter->lastRealCouplingStepTime = time;
        extFilter->lastRealCouplingStepValue = value;

        if (time < MCX_DEBUG_LOG_TIME) {
            MCX_DEBUG_LOG("[%f] Connection: ExtFilter: F SET (%x) (%f, %f)", time, filter, time, value);
        }
    }

    return RETURN_OK;
}

static ChannelValueData ExtFilterGetValue(ChannelFilter * filter, double time) {
    ExtFilter * extFilter = (ExtFilter *) filter;
    ChannelValueData value;

    if (time < MCX_DEBUG_LOG_TIME) {
        MCX_DEBUG_LOG("[%f] Connection: ExtFilter: F GET (%x) (%f, %f)", time, filter, time, extFilter->value);
    }

    if (mcx_poly_get_n(extFilter->polyStruct) != 0) {
        int i = 0;
        for (i = mcx_poly_get_n(extFilter->polyStruct) - 1; i >= 0; i--) {
                if (time == mcx_poly_get_x(extFilter->polyStruct, i)) {
                    value.d = mcx_poly_get_y(extFilter->polyStruct, i);
                return value;
            } else if (time > mcx_poly_get_x(extFilter->polyStruct, i)) {
                break;
            }
        }

        mcx_poly_evaluate_poly(extFilter->polyStruct, time, &extFilter->value, 0);
        value.d = extFilter->value;
    } else {
        mcx_log(LOG_WARNING, "Connection: ExtFilter: Cannot evaluate empty polynomial");
        value.d = extFilter->value;
        return value;
    }

#ifdef MCX_DEBUG
    /* if value == NaN */
    if (value.d != value.d) {
        mcx_log(LOG_WARNING, "Connection: ExtFilter: Polynomial evaluation returned NaN");
        return value;
    }
#endif

    return value;
}

static McxStatus ExtFilterEnterCouplingStepMode(ChannelFilter * filter,
                                                double communicationTimeStepSize,
                                                double sourceTimeStepSize,
                                                double targetTimeStepSize) {
    ExtFilter * extFilter = (ExtFilter *) filter;

    if (InCouplingStepMode == * filter->state) {
        MCX_DEBUG_LOG("Connection: ExtFilter: EnterCoupling: Already in coupling mode");
        return RETURN_OK;
    }

    return RETURN_OK;
}

static McxStatus ExtFilterEnterCommunicationMode(ChannelFilter * filter, double _time) {
    ExtFilter * extFilter = (ExtFilter *) filter;

    double time = extFilter->lastRealCouplingStepTime;
    double value = extFilter->lastRealCouplingStepValue;

    int ret = 0;

    MCX_DEBUG_LOG("Connection: ExtFilter: EnterSynchronization");

    if (mcx_poly_get_n(extFilter->polyStruct) > 0 &&
        mcx_poly_get_x(extFilter->polyStruct, mcx_poly_get_n(extFilter->polyStruct) - 1) == time) {
        // replace last point
        MCX_DEBUG_LOG("[%f] Connection: ExtFilter: Last point replaced (%x) (%f, %f)", time, filter, time, value);
        if (mcx_poly_get_n(extFilter->polyStruct) > 1) {
            mcx_poly_remove_last_points(extFilter->polyStruct, 1);
            ret = mcx_poly_add_points(extFilter->polyStruct, &time, &value, 1);
            if (ret == EXIT_FAILURE) {
                mcx_log(LOG_ERROR, "Connection: ExtFilter: Memory allocation for adding data points failed");
                return RETURN_ERROR;
            }
        } else {
            mcx_poly_shift_points(extFilter->polyStruct, &time, &value, 1);
        }
    } else if (extFilter->n < (extFilter->degree + 1)) {
        // append point
        ret = mcx_poly_add_points(extFilter->polyStruct, &time, &value, 1);
        if (ret == EXIT_FAILURE) {
            mcx_log(LOG_ERROR, "Connection: ExtFilter: Memory allocation for adding data points failed");
            return RETURN_ERROR;
        }
        extFilter->n++;
    } else {
        // shift point
        mcx_poly_shift_points(extFilter->polyStruct, &time, &value, 1);
    }

    ret = mcx_poly_calc_coef_N2(extFilter->polyStruct);
    if (ret == EXIT_FAILURE) {
        mcx_log(LOG_ERROR, "Connection: ExtFilter: Memory allocation for interpolation failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ExtFilterSetup(ExtFilter * filter, int degree) {
    filter->degree = degree;

    return RETURN_OK;
}

static void ExtFilterDestructor(ChannelFilter * filter) {
    ExtFilter * extFilter = (ExtFilter *) filter;

    mcx_poly_free_poly(extFilter->polyStruct);
}

static ExtFilter * ExtFilterCreate(ExtFilter * extFilter) {
    ChannelFilter * filter = (ChannelFilter *) extFilter;

    filter->GetValue = ExtFilterGetValue;
    filter->SetValue = ExtFilterSetValue;

    filter->EnterCommunicationMode = ExtFilterEnterCommunicationMode;
    filter->EnterCouplingStepMode = ExtFilterEnterCouplingStepMode;

    extFilter->Setup = ExtFilterSetup;

    extFilter->value = 0.0;
    extFilter->degree = 0;

    extFilter->n = 0;

    extFilter->polyStruct = mcx_poly_create_poly(4);

    extFilter->lastRealCouplingStepTime = 0.0;
    extFilter->lastRealCouplingStepValue = 0.0;

    return extFilter;
}

OBJECT_CLASS(ExtFilter, ChannelFilter);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */