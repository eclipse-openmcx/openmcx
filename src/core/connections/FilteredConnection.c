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
#include "core/connections/FilteredConnection_impl.h"
#include "core/connections/Connection.h"
#include "core/connections/ConnectionInfo.h"
#include "core/channels/Channel.h"
#include "core/channels/ChannelInfo.h"
#include "core/connections/filters/DiscreteFilter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static FilteredConnectionData * FilteredConnectionDataCreate(FilteredConnectionData * data) {
    data->filter = NULL;

    ChannelValueInit(&data->store, CHANNEL_UNKNOWN);

    return data;
}

static void FilteredConnectionDataDestructor(FilteredConnectionData * data) {
    ChannelValueDestructor(&data->store);
    object_destroy(data->filter);
}

OBJECT_CLASS(FilteredConnectionData, Object);


static McxStatus FilteredConnectionSetup(Connection * connection, ChannelOut * out,
                                         ChannelIn * in, ConnectionInfo * info) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;

    ChannelInfo * sourceInfo = &((Channel *)out)->info;
    ChannelInfo * targetInfo = &((Channel *)in)->info;

    McxStatus retVal = RETURN_OK;

    // Decoupling
    if (DECOUPLE_ALWAYS == info->decoupleType) {
        ConnectionInfoSetDecoupled(info);
    }

    // filter will be added after model is connected
    filteredConnection->data->filter = NULL;

    // value store
    ChannelValueInit(&filteredConnection->data->store, sourceInfo->type);

    // value reference
    connection->value_ = ChannelValueReference(&filteredConnection->data->store);


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
    ChannelFilter * filter = filteredConnection->GetWriteFilter(filteredConnection);

    McxStatus retVal = RETURN_OK;

    if (filter) {
        if (filter->EnterCommunicationMode) {
            retVal = filter->EnterCommunicationMode(filter, time);
            if (RETURN_OK != retVal) {
                return RETURN_ERROR;
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
    ChannelFilter * filter = filteredConnection->GetWriteFilter(filteredConnection);

    McxStatus retVal = RETURN_OK;

    if (filter) {
        if (filter->EnterCouplingStepMode) {
            retVal = filter->EnterCouplingStepMode(filter, communicationTimeStepSize, sourceTimeStepSize, targetTimeStepSize);
            if (RETURN_OK != retVal) {
                return RETURN_ERROR;
            }
        }
    }

    connection->state_ = InCouplingStepMode;
    return RETURN_OK;
}

static ChannelFilter * FilteredConnectionGetFilter(FilteredConnection * connection) {
    return connection->data->filter;
}

static void FilteredConnectionSetResult(FilteredConnection * connection, const void * value) {
    ChannelValueSetFromReference(&connection->data->store, value);
}

static void FilteredConnectionUpdateFromInput(Connection * connection, TimeInterval * time) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;
    ChannelFilter * filter = filteredConnection->GetWriteFilter(filteredConnection);
    Channel * channel = (Channel *) connection->GetSource(connection);

#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        ChannelInfo * info = &channel->info;
        MCX_DEBUG_LOG("[%f] FCONN   (%s) UpdateFromInput", time->startTime, ChannelInfoGetName(info));
    }
#endif

    if (filter && time->startTime >= 0) {
        ChannelValueData value = * (ChannelValueData *) channel->GetValueReference(channel);
        filter->SetValue(filter, time->startTime, value);
    }
}

static void FilteredConnectionUpdateToOutput(Connection * connection, TimeInterval * time) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;

    ChannelFilter * filter = NULL;

    Channel * channel = (Channel *) connection->GetSource(connection);
    ChannelOut * out  = (ChannelOut *) channel;

#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        ChannelInfo * info = &channel->info;
        MCX_DEBUG_LOG("[%f] FCONN   (%s) UpdateToOutput", time->startTime, ChannelInfoGetName(info));
    }
#endif

    if (out->GetFunction(out)) {
        proc * p = (proc *) out->GetFunction(out);
        double value = 0.0;

        value = p->fn(time, p->env);
        filteredConnection->SetResult(filteredConnection, &value);
    } else {
        // only filter if time is not negative (negative time means filter disabled)
        if (filteredConnection->GetReadFilter(filteredConnection) && time->startTime >= 0) {
            ChannelValueData value;

            filter = filteredConnection->GetReadFilter(filteredConnection);
            value = filter->GetValue(filter, time->startTime);

            filteredConnection->SetResult(filteredConnection, &value);
        }
    }
}

static McxStatus AddFilter(Connection * connection) {
    FilteredConnection * filteredConnection = (FilteredConnection *) connection;
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    if (filteredConnection->data->filter) {
        mcx_log(LOG_DEBUG, "Connection: Not inserting filter");
    } else {
        filteredConnection->data->filter = FilterFactory(connection);
        if (NULL == filteredConnection->data->filter) {
            mcx_log(LOG_DEBUG, "Connection: No Filter created");
            retVal = RETURN_ERROR;
        }
    }

    return retVal;
}

static void FilteredConnectionDestructor(FilteredConnection * filteredConnection) {
    object_destroy(filteredConnection->data);
}

static FilteredConnection * FilteredConnectionCreate(FilteredConnection * filteredConnection) {
    Connection * connection = (Connection *) filteredConnection;

    connection->Setup    = FilteredConnectionSetup;
    connection->UpdateFromInput = FilteredConnectionUpdateFromInput;
    connection->UpdateToOutput  = FilteredConnectionUpdateToOutput;

    connection->EnterCommunicationMode = FilteredConnectionEnterCommunicationMode;
    connection->EnterCouplingStepMode     = FilteredConnectionEnterCouplingStepMode;

    connection->AddFilter = AddFilter;

    filteredConnection->GetReadFilter  = FilteredConnectionGetFilter;
    filteredConnection->GetWriteFilter = FilteredConnectionGetFilter;

    filteredConnection->SetResult = FilteredConnectionSetResult;

    filteredConnection->data = (FilteredConnectionData *) object_create(FilteredConnectionData);

    return filteredConnection;
}

OBJECT_CLASS(FilteredConnection, Connection);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */