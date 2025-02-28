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
#include "core/connections/filters/IntFilter.h"
#include "core/Interpolation.h"
#include "util/compare.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus IntFilterSetValue(ChannelFilter * filter, double time, ChannelValueData _value){
    IntFilter * intFilter = (IntFilter *) filter;

    double value = _value.d;

    double dtime;

    dtime = time - intFilter->lastCouplingStepTime;

    if (0.0 == dtime) { // should be binary 0 since no operation should be performed on time
        MCX_DEBUG_LOG("Connection: IntFilter (%p): dtime == 0 at time: %.17g vs. %.17g", filter, time, intFilter->lastCouplingStepTime);
        return RETURN_OK;
    }

    if (intFilter->nWriteCouplingSteps > 0) {
        MCX_DEBUG_LOG("Connection: IntFilter: %s: SetValue %f Time: %.17g, [%f,%f]",
                      (InCommunicationMode != * filter->state ? "CouplingStep" : "SynchronizationStep"),
                      value, time,
                      intFilter->write_x_data[0],
                      intFilter->write_x_data[intFilter->nWriteCouplingSteps - 1]
                      );
    } else {
        MCX_DEBUG_LOG("Connection: IntFilter: %s: SetValue %f Time: %.17g",
                      (InCommunicationMode != * filter->state ? "CouplingStep" : "SynchronizationStep"),
                      value, time);
    }

    intFilter->lastCouplingStepTime = time;

    if (intFilter->nWriteCouplingSteps > 0
        && double_eq(intFilter->write_x_data[intFilter->nWriteCouplingSteps - 1], time)) {
        mcx_log(LOG_DEBUG, "Connection: IntFilter: Value already set for time %.17g", time);
        return RETURN_OK;
    }

    if (intFilter->nWriteCouplingSteps < intFilter->dataLen) {
        intFilter->write_x_data[intFilter->nWriteCouplingSteps] = time;
        intFilter->write_y_data[intFilter->nWriteCouplingSteps] = value;
        intFilter->nWriteCouplingSteps++;
    } else {
        mcx_log(LOG_WARNING, "Connection: IntFilter: SetValue: Number of stored values larger than buffer size");
        intFilter->write_x_data[intFilter->dataLen-1] = time;
        intFilter->write_y_data[intFilter->dataLen-1] = value;
    }

    return RETURN_OK;
}

static ChannelValueData IntFilterGetValue(ChannelFilter * filter, double time){
    IntFilter * intFilter = (IntFilter *) filter;

    ChannelValueData value;

    // TODO: investigate performance impact of the comparisons and remove the ifdef if feasible
#ifdef MCX_DEBUG
    if (double_lt(time, intFilter->read_x_data[0]) ||
        (intFilter->nReadCouplingSteps > 0
         && double_gt(time, intFilter->read_x_data[intFilter->nReadCouplingSteps -1]))) {
        if (intFilter->nReadCouplingSteps > 0) {
            double t1 = intFilter->read_x_data[0];
            double t2 = intFilter->read_x_data[intFilter->nReadCouplingSteps -1];
            mcx_log(LOG_WARNING, "Connection: IntFilter %p: Extrapol. with interp. filter (time=%.4f, nReadCouplingSteps=%d, [%.4f,%.4f])", filter, time, intFilter->nReadCouplingSteps, t1, t2);
        } else {
            mcx_log(LOG_WARNING, "Connection: IntFilter %p: Extrapol. with interp. filter (time=%.4f, nReadCouplingSteps=%d)", filter, time, intFilter->nReadCouplingSteps);
        }
    }
#endif

    value.d = mcx_interp_get_value_from_table(intFilter->T, time);

#ifdef MCX_DEBUG
    if (intFilter->nReadCouplingSteps > 0) {
        MCX_DEBUG_LOG("Connection: IntFilter: GetValue: time=%.17g, value=%f, [%f,%f]",
                      time, value.d,
                      intFilter->read_x_data[0],
                      intFilter->read_x_data[intFilter->nReadCouplingSteps - 1]);
    } else {
        MCX_DEBUG_LOG("Connection: IntFilter: GetValue: time=%.17g, value=%f", time, value.d);
    }

    /* if value == NaN */
    if (value.d != value.d) {
        mcx_log(LOG_WARNING, "Connection: IntFilter: polynomial evaluation returned NaN");
        return value;
    }
#endif

    return value;
}

static McxStatus IntFilterEnterCouplingStepMode(ChannelFilter * filter
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize)
{
    IntFilter * intFilter = (IntFilter *) filter;

    if (InCouplingStepMode == * filter->state) {
        MCX_DEBUG_LOG("Connection: IntFilter: EnterCoupling: already in coupling");
        return RETURN_OK;
    }

    MCX_DEBUG_LOG("Connection: IntFilter: EnterCoupling: synchronization step=%f", communicationTimeStepSize);

    return RETURN_OK;
}

static McxStatus IntFilterEnterCommunicationMode(ChannelFilter * filter, double time) {
    IntFilter * intFilter = (IntFilter *) filter;

    double * tmp_data = NULL;

    if (InCommunicationMode == * filter->state) {
        MCX_DEBUG_LOG("Connection: IntFilter: EnterSynchronization: already in synchronization mode");
        return RETURN_OK;
    }

    MCX_DEBUG_LOG("Connection: IntFilter: EnterSynchronization");

    /* switch read and write vectors */
    if (intFilter->nReadCouplingSteps == 0 || double_gt(time, intFilter->read_x_data[intFilter->nReadCouplingSteps - 1])) {
        mcx_interp_change_table_data(intFilter->T, intFilter->write_x_data,
            intFilter->write_y_data, (int)intFilter->nWriteCouplingSteps);

        intFilter->nReadCouplingSteps = intFilter->nWriteCouplingSteps;

        tmp_data = intFilter->write_x_data;
        intFilter->write_x_data = intFilter->read_x_data;
        intFilter->read_x_data = tmp_data;

        tmp_data = intFilter->write_y_data;
        intFilter->write_y_data = intFilter->read_y_data;
        intFilter->read_y_data = tmp_data;

        /* save last synchronization step value */
        if (intFilter->nReadCouplingSteps > 0) {
            intFilter->write_x_data[0] = intFilter->read_x_data[intFilter->nReadCouplingSteps - 1];
            intFilter->write_y_data[0] = intFilter->read_y_data[intFilter->nReadCouplingSteps - 1];
            intFilter->nWriteCouplingSteps = 1;
        } else {
            intFilter->nWriteCouplingSteps = 0;
        }
    }

    return RETURN_OK;
}

static McxStatus IntFilterSetup(IntFilter * intFilter, int degree, size_t buffSize){
    ChannelFilter * filter = (ChannelFilter *) intFilter;

    mcx_table_interp_type intType = MCX_TABLE_INTERP_NOT_SET;
    mcx_table_extrap_type extType = MCX_TABLE_EXTRAP_NOT_SET;

    int ret = 0;

    if (buffSize == 0) {
        mcx_log(LOG_ERROR, "IntFilter: Buffer size %zu not supported", buffSize);
        return RETURN_ERROR;
    }

    intFilter->dataLen = buffSize;

    intFilter->read_x_data = (double*)mcx_malloc(intFilter->dataLen * sizeof(double));
    intFilter->read_y_data = (double*)mcx_malloc(intFilter->dataLen * sizeof(double));

    intFilter->write_x_data = (double*)mcx_malloc(intFilter->dataLen * sizeof(double));
    intFilter->write_y_data = (double*)mcx_malloc(intFilter->dataLen * sizeof(double));

    if (!intFilter->read_x_data ||
        !intFilter->read_y_data ||
        !intFilter->write_x_data ||
        !intFilter->write_y_data)
    {
        mcx_log(LOG_ERROR, "IntFilter: Buffer allocation failed");
        return RETURN_ERROR;
    }

    intFilter->couplingPolyDegree = degree;

    switch (degree) {
    case 0:
        intType = MCX_TABLE_INTERP_STEP_RIGHT;
        extType = MCX_TABLE_EXTRAP_CONST;
        break;
    case 1:
        intType = MCX_TABLE_INTERP_LINEAR;
        extType = MCX_TABLE_EXTRAP_LINEAR;
        break;
    default:
        mcx_log(LOG_ERROR, "IntFilter: Degree %d not supported", degree);
        return RETURN_ERROR;
    }

    ret = mcx_interp_setup_table(intFilter->T, intType, extType);
    if (EXIT_FAILURE == ret) {
        mcx_log(LOG_ERROR, "IntFilter: Setting up table failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void IntFilterDestructor(IntFilter * filter){
    mcx_interp_free_table(filter->T);

    if (filter->read_x_data) { mcx_free(filter->read_x_data); }
    if (filter->read_y_data) { mcx_free(filter->read_y_data); }

    if (filter->write_x_data) { mcx_free(filter->write_x_data); }
    if (filter->write_y_data) { mcx_free(filter->write_y_data); }
}

static IntFilter * IntFilterCreate(IntFilter * intFilter){
    ChannelFilter * filter = (ChannelFilter *) intFilter;

    filter->EnterCommunicationMode = IntFilterEnterCommunicationMode;
    filter->EnterCouplingStepMode = IntFilterEnterCouplingStepMode;

    filter->GetValue = IntFilterGetValue;
    filter->SetValue = IntFilterSetValue;

    intFilter->Setup = IntFilterSetup;

    /* init last step time with negative value as to not ignore value at 0.0 */
    intFilter->lastCouplingStepTime = -1.0;
    intFilter->nWriteCouplingSteps = 0;
    intFilter->nReadCouplingSteps = 0;

    intFilter->T = mcx_interp_new_table();

    intFilter->dataLen = 0;

    intFilter->read_x_data = NULL;
    intFilter->read_y_data = NULL;

    intFilter->write_x_data = NULL;
    intFilter->write_y_data = NULL;

    return intFilter;
}

OBJECT_CLASS(IntFilter, ChannelFilter);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */