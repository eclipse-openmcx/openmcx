/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/connections/FilteredConnection.h"
#include "core/connections/Connection.h"
#include "core/connections/ConnectionInfo.h"
#include "core/channels/Channel.h"
#include "core/channels/ChannelInfo.h"
#include "core/connections/filters/DiscreteFilter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static McxStatus FilteredConnectionDataInit(FilteredConnectionData * data) {
    data->filters = NULL;
    data->_filter = NULL;
    data->numFilters = 0;

    ChannelValueInit(&data->updateBuffer, ChannelTypeClone(&ChannelTypeUnknown));
    ChannelValueInit(&data->store, ChannelTypeClone(&ChannelTypeUnknown));

    return RETURN_OK;
}

static void FilteredConnectionDataDestructor(FilteredConnectionData * data) {
    ChannelValueDestructor(&data->store);
    ChannelValueDestructor(&data->updateBuffer);

    if (data->filters) {
        size_t i = 0;
        for (i = 0; i < data->numFilters; i++) {
            object_destroy(data->filters[i]);
        }
        if (data->numFilters != 1) {
            mcx_free(data->filters);
        }
    }
}

static McxStatus FilteredConnectionSetup(Connection * connection, ChannelOut * out,
                                         ChannelIn * in, ConnectionInfo * info) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;

    ChannelInfo * sourceInfo = &((Channel *)out)->info;
    ChannelInfo * targetInfo = &((Channel *)in)->info;

    ChannelType * storeType = NULL;

    McxStatus retVal = RETURN_OK;

    // Decoupling
    if (DECOUPLE_ALWAYS == info->decoupleType) {
        ConnectionInfoSetDecoupled(info);
    }

    // filter will be added after model is connected
    filteredConnection->data.filters = NULL;
    filteredConnection->data.numFilters = 0;

    storeType = ChannelTypeFromDimension(sourceInfo->type, info->sourceDimension);
    if (!storeType) {
        return RETURN_ERROR;
    }

    // value store
    ChannelValueInit(&filteredConnection->data.store, storeType);  // steals ownership of storeType -> no clone needed

    // value reference
    connection->value_ = ChannelValueDataPointer(&filteredConnection->data.store);

    // initialize the buffer for channel function calls
    ChannelValueInit(&filteredConnection->data.updateBuffer, ChannelTypeClone(storeType));

    // Connection::Setup()
    // this has to be done last as it connects the channels
    retVal = ConnectionSetup(connection, out, in, info);
    if (RETURN_OK != retVal) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus FilteredConnectionEnterCommunicationMode(Connection * connection, double time) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;

    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    for (i = 0; i < filteredConnection->data.numFilters; i++) {
        ChannelFilter * filter = filteredConnection->GetWriteFilter(filteredConnection, i);
        if (filter) {
            if (filter->EnterCommunicationMode) {
                retVal = filter->EnterCommunicationMode(filter, time);
                if (RETURN_OK != retVal) {
                    return RETURN_ERROR;
                }
            }
        }
    }

    connection->state_ = InCommunicationMode;
    return RETURN_OK;
}

static McxStatus FilteredConnectionEnterCouplingStepMode(Connection * connection
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize)
{
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;

    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    for (i = 0; i < filteredConnection->data.numFilters; i++) {
        ChannelFilter * filter = filteredConnection->GetWriteFilter(filteredConnection, i);
        if (filter) {
            if (filter->EnterCouplingStepMode) {
                retVal = filter->EnterCouplingStepMode(filter, communicationTimeStepSize, sourceTimeStepSize, targetTimeStepSize);
                if (RETURN_OK != retVal) {
                    return RETURN_ERROR;
                }
            }
        }
    }

    connection->state_ = InCouplingStepMode;
    return RETURN_OK;
}

static ChannelFilter * FilteredConnectionGetFilter(FilteredConnection * connection, size_t idx) {
    if (connection->data.filters && idx < connection->data.numFilters) {
        return connection->data.filters[idx];
    }
    return NULL;
}

static size_t FilteredConnectionGetNumFilters(FilteredConnection *connection) {
    return connection->data.numFilters;
}

static McxStatus FilteredConnectionSetResult(FilteredConnection * connection, const void * value) {
    return ChannelValueSetFromReference(&connection->data.store, value);
}

static size_t GetSliceShift(Connection * connection) {
    Channel * channel = (Channel *) connection->GetSource(connection);
    ChannelInfo * channelInfo = &channel->info;
    ChannelDimension * sourceDimension = channelInfo->dimension;

    ConnectionInfo * connInfo = connection->GetInfo(connection);
    ChannelDimension * sliceDimension = connInfo->sourceDimension;

    // only 1D at the moment
    return sliceDimension->startIdxs[0] - sourceDimension->startIdxs[0];
}

static void FilteredConnectionUpdateFromInput(Connection * connection, TimeInterval * time) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;
    Channel * channel = (Channel *) connection->GetSource(connection);
    ChannelInfo * info = &channel->info;

#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        MCX_DEBUG_LOG("[%f] FCONN   (%s) UpdateFromInput", time->startTime, ChannelInfoGetName(info));
    }
#endif

    if (ChannelTypeIsScalar(info->type)) {
        ChannelFilter * filter = filteredConnection->GetWriteFilter(filteredConnection, 0);
        if (filter && time->startTime >= 0) {
            ChannelValueData value = *(ChannelValueData *) channel->GetValueReference(channel);
            filter->SetValue(filter, time->startTime, value);
        }
    } else {
        size_t i = 0;
        size_t shift = GetSliceShift(connection);
        ChannelValueData element;
        ChannelValueDataInit(&element, ChannelTypeBaseType(info->type));

        for (i = 0; i < FilteredConnectionGetNumFilters(filteredConnection); i++) {
            ChannelFilter * filter = filteredConnection->GetWriteFilter(filteredConnection, i);

            if (filter && time->startTime >= 0) {
                ChannelValueData value = *(ChannelValueData *) channel->GetValueReference(channel);
                mcx_array_get_elem(&value.a, i + shift, &element);
                filter->SetValue(filter, time->startTime, element);
            }
        }
    }
}

static McxStatus FilteredConnectionUpdateToOutput(Connection * connection, TimeInterval * time) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;

    ChannelFilter * filter = NULL;

    Channel * channel = (Channel *) connection->GetSource(connection);
    ChannelInfo * info = &channel->info;
    ChannelOut * out  = (ChannelOut *) channel;

#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        MCX_DEBUG_LOG("[%f] FCONN   (%s) UpdateToOutput", time->startTime, ChannelInfoGetName(info));
    }
#endif

    if (out->GetFunction(out)) {
        proc * p = (proc *) out->GetFunction(out);

        // TODO: Update functions to only update the slices ?
        if (RETURN_ERROR == p->fn(time, p->env, &filteredConnection->data.updateBuffer)) {
            mcx_log(LOG_ERROR, "FilteredConnection: Function failed");
            return RETURN_ERROR;
        }

        if (RETURN_OK != filteredConnection->SetResult(filteredConnection, ChannelValueDataPointer(&filteredConnection->data.updateBuffer))) {
            mcx_log(LOG_ERROR, "FilteredConnection: SetResult failed");
            return RETURN_ERROR;
        }
    } else {
        // only filter if time is not negative (negative time means filter disabled)
        if (filteredConnection->GetReadFilter(filteredConnection, 0) && time->startTime >= 0) {
            ChannelValueData value = { 0 };

            if (ChannelTypeIsScalar(info->type)) {
                filter = filteredConnection->GetReadFilter(filteredConnection, 0);
                value = filter->GetValue(filter, time->startTime);

                if (RETURN_OK != filteredConnection->SetResult(filteredConnection, &value)) {
                    mcx_log(LOG_ERROR, "FilteredConnection: SetResult failed");
                    return RETURN_ERROR;
                }
            } else {
                size_t i = 0;
                size_t numFilters = FilteredConnectionGetNumFilters(filteredConnection);
                ChannelType * type = info->type;

                mcx_array * elements = (mcx_array *) ChannelValueDataPointer(&filteredConnection->data.updateBuffer);
                char * dest = (char *) elements->data;
                for (i = 0; i < numFilters; i++) {
                    filter = filteredConnection->GetReadFilter(filteredConnection, i);
                    value = filter->GetValue(filter, time->startTime);

                    ChannelValueDataSetToReference(&value, ChannelTypeBaseType(type), (void *) dest);
                    dest += ChannelValueTypeSize(ChannelTypeBaseType(type));
                }

                if (RETURN_OK != filteredConnection->SetResult(filteredConnection, elements)) {
                    mcx_log(LOG_ERROR, "FilteredConnection: SetResult failed");
                    return RETURN_ERROR;
                }
            }
        }
    }

    return RETURN_OK;
}

static McxStatus AddFilter(Connection * connection) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    if (filteredConnection->data.filters) {
        mcx_log(LOG_DEBUG, "Connection: Not inserting filter");
    } else {
        ConnectionInfo * info = connection->GetInfo(connection);
        const char * connString = ConnectionInfoConnectionString(info);
        ChannelDimension * dimension = info->sourceDimension;

        if (dimension) {    // array
            size_t i = 0;

            if (dimension->num > 1) {
                mcx_log(LOG_ERROR, "Setting filters for multi-dimensional connections is not supported");
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            filteredConnection->data.numFilters = dimension->endIdxs[0] - dimension->startIdxs[0] + 1;
            filteredConnection->data.filters = (ChannelFilter **) mcx_calloc(filteredConnection->data.numFilters, sizeof(ChannelFilter*));
            if (!filteredConnection->data.filters) {
                mcx_log(LOG_ERROR, "Creating array filters failed: no memory");
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            for (i = 0; i < filteredConnection->data.numFilters; i++) {
                filteredConnection->data.filters[i] = FilterFactory(&connection->state_,
                                                                     info->interExtrapolationType,
                                                                     &info->interExtrapolationParams,
                                                                     ChannelTypeBaseType(ConnectionInfoGetType(info)),
                                                                     info->isInterExtrapolating,
                                                                     ConnectionInfoIsDecoupled(info),
                                                                     info->sourceComponent,
                                                                     info->targetComponent,
                                                                     connString);
                if (NULL == filteredConnection->data.filters[i]) {
                    mcx_log(LOG_DEBUG, "Connection: Array filter creation failed for index %d", i);
                    retVal = RETURN_ERROR;
                    goto cleanup;
                }
            }
        } else {
            filteredConnection->data.numFilters = 1;
            filteredConnection->data._filter = FilterFactory(&connection->state_,
                                                             info->interExtrapolationType,
                                                             &info->interExtrapolationParams,
                                                             ConnectionInfoGetType(info),
                                                             info->isInterExtrapolating,
                                                             ConnectionInfoIsDecoupled(info),
                                                             info->sourceComponent,
                                                             info->targetComponent,
                                                             connString);
            if (NULL == filteredConnection->data._filter) {
                mcx_log(LOG_DEBUG, "Connection: No Filter created");
                retVal = RETURN_ERROR;
                goto cleanup;
            }
            filteredConnection->data.filters = &filteredConnection->data._filter;
        }

cleanup:
        mcx_free(connString);
        if (retVal != RETURN_OK) {
            return retVal;
        }
    }

    return RETURN_OK;
}

static ChannelType * FilteredConnectionGetValueType(Connection * connection) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;
    return filteredConnection->data.store.type;
}

static void FilteredConnectionDestructor(FilteredConnection * filteredConnection) {
    FilteredConnectionDataDestructor(&filteredConnection->data);
}

static FilteredConnection * FilteredConnectionCreate(FilteredConnection * filteredConnection) {
    Connection * connection = (Connection *) filteredConnection;
    McxStatus retVal = RETURN_OK;

    connection->Setup    = FilteredConnectionSetup;
    connection->UpdateFromInput = FilteredConnectionUpdateFromInput;
    connection->UpdateToOutput  = FilteredConnectionUpdateToOutput;

    connection->EnterCommunicationMode = FilteredConnectionEnterCommunicationMode;
    connection->EnterCouplingStepMode     = FilteredConnectionEnterCouplingStepMode;

    connection->AddFilter = AddFilter;
    connection->GetValueType = FilteredConnectionGetValueType;

    filteredConnection->GetReadFilter  = FilteredConnectionGetFilter;
    filteredConnection->GetWriteFilter = FilteredConnectionGetFilter;

    filteredConnection->SetResult = FilteredConnectionSetResult;

    retVal = FilteredConnectionDataInit(&filteredConnection->data);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "FilteredConnectionCreate: FilteredConnectionDataInit failed");
        return NULL;
    }

    return filteredConnection;
}

OBJECT_CLASS(FilteredConnection, Connection);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */