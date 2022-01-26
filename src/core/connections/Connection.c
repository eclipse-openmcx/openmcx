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
#include "core/connections/Connection.h"
#include "core/connections/Connection_impl.h"
#include "core/channels/Channel.h"
#include "core/connections/ConnectionInfo.h"
#include "core/Conversion.h"

#include "core/Databus.h"
#include "core/Component.h"
#include "core/Model.h"

// Filter
#include "core/connections/filters/DiscreteFilter.h"
#include "core/connections/filters/IntExtFilter.h"
#include "core/connections/filters/ExtFilter.h"
#include "core/connections/filters/IntFilter.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

McxStatus CheckConnectivity(ObjectContainer * connections) {
    size_t i = 0;

    size_t connSize = connections->Size(connections);

    for (i = 0; i < connSize; i++) {
        ConnectionInfo * connInfo = (ConnectionInfo *) connections->At(connections, i);
        ChannelInfo * info = NULL;
        Component * target = connInfo->GetTargetComponent(connInfo);
        int targetId = connInfo->GetTargetChannelID(connInfo);

        Component * source = connInfo->GetSourceComponent(connInfo);
        int sourceId = connInfo->GetSourceChannelID(connInfo);

        info = DatabusInfoGetChannel(DatabusGetInInfo(target->GetDatabus(target)), targetId);
        if (info) {
            info->connected = 1;
        }

        info = DatabusInfoGetChannel(DatabusGetOutInfo(source->GetDatabus(source)), sourceId);
        if (info) {
            info->connected = 1;
        }
    }

    return RETURN_OK;
}

McxStatus MakeOneConnection(ConnectionInfo * info, InterExtrapolatingType isInterExtrapolating) {
    Component * source = NULL;
    Component * target = NULL;

    Connection * connection = NULL;

    ChannelInfo * outInfo = NULL;
    ChannelInfo * inInfo = NULL;

    source = info->GetSourceComponent(info);
    target = info->GetTargetComponent(info);

    // Get data types of involved channels
    outInfo = DatabusInfoGetChannel(DatabusGetOutInfo(source->GetDatabus(source)),
        info->GetSourceChannelID(info));

    inInfo = DatabusInfoGetChannel(DatabusGetInInfo(target->GetDatabus(target)),
        info->GetTargetChannelID(info));

    if (!outInfo || !inInfo) {
        mcx_log(LOG_ERROR, "Connection: Make connection: Invalid arguments");
        return RETURN_ERROR;
    }

    InterExtrapolationParams * params = info->GetInterExtraParams(info);

    if (EXTRAPOLATING == isInterExtrapolating) {
        if (params->extrapolationOrder != params->interpolationOrder) {
            isInterExtrapolating = INTEREXTRAPOLATING;
        }
    } else if (INTERPOLATING == isInterExtrapolating) {
        isInterExtrapolating = INTEREXTRAPOLATING;
    }

    info->SetInterExtrapolating(info, isInterExtrapolating);

    connection = DatabusCreateConnection(source->GetDatabus(source), info);
    if (!connection) {
        mcx_log(LOG_ERROR, "Connection: Make connection: Could not create connection");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void LogStepRatios(double sourceStep, double targetStep, double synchStep, ConnectionInfo * info) {
    char * connString = info->ConnectionString(info);

    if (sourceStep <= synchStep && targetStep <= synchStep) {
        mcx_log(LOG_DEBUG, "%s: source <= synch && target <= synch", connString);
    } else if (sourceStep <= synchStep && targetStep > synchStep) {
        mcx_log(LOG_INFO, "CONN %s: source <= synch && target > synch", connString);
    } else if (sourceStep > synchStep && targetStep <= synchStep) {
        mcx_log(LOG_INFO, "CONN %s: source > synch && target <= synch", connString);
    } else {
        mcx_log(LOG_INFO, "CONN %s: source > synch && target > synch", connString);
    }

    mcx_free(connString);
}

static int ComponentMightNotRespectStepSize(Component * comp) {
    return FALSE;
}

static size_t DetermineFilterBufferSize(ConnectionInfo * info) {
    Component * source = info->GetSourceComponent(info);
    Component * target = info->GetTargetComponent(info);

    Model * model = source->GetModel(source);
    Task * task = model->GetTask(model);

    double synchStep = task->GetTimeStep(task);
    double sourceStep = source->GetTimeStep(source) > 0 ? source->GetTimeStep(source) : synchStep;
    double targetStep = target->GetTimeStep(target) > 0 ? target->GetTimeStep(target) : synchStep;

    size_t buffSize = 0;

    if (model->config->overrideInterpolationBuffSize > 0) {
        buffSize = model->config->overrideInterpolationBuffSize;
    }
    else if (ComponentMightNotRespectStepSize(source) || ComponentMightNotRespectStepSize(target)) {
        buffSize = model->config->interpolationBuffSize;
    }
    else {
        buffSize = ceil(synchStep / sourceStep) + 1;
    }

    buffSize += model->config->interpolationBuffSizeSafetyExt;

    if (buffSize > model->config->interpolationBuffSizeLimit) {
        char * connString = info->ConnectionString(info);
        mcx_log(LOG_WARNING, "%s: buffer limit exceeded (%zu > &zu). Limit can be changed via MC_INTERPOLATION_BUFFER_SIZE_LIMIT.",
                connString, buffSize, model->config->interpolationBuffSizeLimit);
        mcx_free(connString);

        buffSize = model->config->interpolationBuffSizeLimit;
    }

    return buffSize;
}


ChannelFilter * FilterFactory(Connection * connection) {
    ChannelFilter * filter = NULL;
    McxStatus retVal;
    ConnectionInfo * info = connection->GetInfo(connection);

    InterExtrapolationType extrapolType = info->GetInterExtraType(info);
    InterExtrapolationParams * params = info->GetInterExtraParams(info);

    if (info->GetType(info) == CHANNEL_DOUBLE) {
        if (!(INTERVAL_COUPLING == params->interpolationInterval && INTERVAL_SYNCHRONIZATION == params->extrapolationInterval)) {
            mcx_log(LOG_WARNING, "The use of inter/extrapolation interval settings for double is not supported");
        }
        if (extrapolType == INTEREXTRAPOLATION_POLYNOMIAL) {

            InterExtrapolatingType isInterExtrapol = info->GetInterExtrapolating(info);
            if (INTERPOLATING == isInterExtrapol && info->IsDecoupled(info)) {
                isInterExtrapol = INTEREXTRAPOLATING;
            }

            int degree = (INTERPOLATING == isInterExtrapol) ? params->interpolationOrder : params->extrapolationOrder;

            if (EXTRAPOLATING == isInterExtrapol || INTEREXTRAPOLATING == isInterExtrapol) {
                    if (INTEREXTRAPOLATING == isInterExtrapol) {
                        IntExtFilter * intExtFilter = (IntExtFilter *)object_create(IntExtFilter);
                        filter = (ChannelFilter *)intExtFilter;
                        mcx_log(LOG_DEBUG, "    Setting up dynamic filter. (%p)", filter);
                        mcx_log(LOG_DEBUG, "    Interpolation order: %d, extrapolation order: %d", params->interpolationOrder, params->extrapolationOrder);
                        size_t buffSize = DetermineFilterBufferSize(info);
                        retVal = intExtFilter->Setup(intExtFilter, params->extrapolationOrder, params->interpolationOrder, buffSize);
                        if (RETURN_OK != retVal) {
                            return NULL;
                        }
                    } else {
                        ExtFilter * extFilter = (ExtFilter *)object_create(ExtFilter);
                        filter = (ChannelFilter *)extFilter;
                        mcx_log(LOG_DEBUG, "    Setting up synchronization step extrapolation filter. (%p)", filter);
                        mcx_log(LOG_DEBUG, "    Extrapolation order: %d", degree);
                        retVal = extFilter->Setup(extFilter, degree);
                        if (RETURN_OK != retVal) {
                            return NULL;
                        }
                    }
            } else {
                IntFilter * intFilter = (IntFilter *) object_create(IntFilter);
                filter = (ChannelFilter *) intFilter;
                mcx_log(LOG_DEBUG, "    Setting up coupling step interpolation filter. (%p)", filter);
                mcx_log(LOG_DEBUG, "    Interpolation order: %d", degree);
                size_t buffSize = DetermineFilterBufferSize(info);
                retVal = intFilter->Setup(intFilter, degree, buffSize);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "Connection: Filter: Could not setup");
                    return NULL;
                }
            }

            if (!filter) {
                mcx_log(LOG_ERROR, "Connection: Filter: Filter creation failed");
                return NULL;
            }
        }
    } else {
        DiscreteFilter * discreteFilter = NULL;

        if (!(0 == params->extrapolationOrder &&
              0 == params->interpolationOrder &&
              INTERVAL_COUPLING == params->interpolationInterval &&
              INTERVAL_SYNCHRONIZATION == params->extrapolationInterval
        )) {
            mcx_log(LOG_WARNING, "Invalid inter/extrapolation settings for non-double connection detected");
        }
        mcx_log(LOG_DEBUG, "Using constant synchronization step extrapolation for non-double connection");

        discreteFilter = (DiscreteFilter *) object_create(DiscreteFilter);
        discreteFilter->Setup(discreteFilter, info->GetType(info));


        filter = (ChannelFilter *) discreteFilter;
    }

    if (NULL == filter && info->GetType(info) == CHANNEL_DOUBLE) {
        // TODO: add a check to avoid filters for non-multirate cases

        ExtFilter * extFilter = (ExtFilter *) object_create(ExtFilter);
        extFilter->Setup(extFilter, 0);
        filter = (ChannelFilter *) extFilter;
    }

    filter->AssignState(filter, &connection->data->state);

    return filter;
}


static ConnectionData * ConnectionDataCreate(ConnectionData * data) {
    data->out = NULL;
    data->in = NULL;

    data->info = NULL;

    data->value = NULL;
    data->useInitialValue = FALSE;

    data->isActiveDependency = TRUE;

    ChannelValueInit(&data->store, CHANNEL_UNKNOWN);

    data->state = InCommunicationMode;

    data->NormalUpdateFrom = NULL;
    data->NormalUpdateTo = NULL;
    data->normalValue = NULL;

    return data;
}


static void ConnectionDataDestructor(ConnectionData * data) {
    object_destroy(data->info);

    ChannelValueDestructor(&data->store);
}

OBJECT_CLASS(ConnectionData, Object);


static void * ConnectionGetValueReference(Connection * connection) {
    return (void *)connection->data->value;
}


static void ConnectionDestructor(Connection * connection) {
    object_destroy(connection->data);
}

static ChannelOut * ConnectionGetSource(Connection * connection) {
    return connection->data->out;
}

static ChannelIn  * ConnectionGetTarget(Connection * connection) {
    return connection->data->in;
}

static ConnectionInfo * ConnectionGetInfo(Connection * connection) {
    return connection->data->info;
}

static int ConnectionIsDecoupled(Connection * connection) {
    ConnectionInfo * info = connection->GetInfo(connection);
    return info->IsDecoupled(info);
}

static int ConnectionIsDefinedDuringInit(Connection * connection) {
    Channel * channel = (Channel *) connection->GetTarget(connection);
    return channel->IsDefinedDuringInit(channel);
}

static void ConnectionSetDefinedDuringInit(Connection * connection) {
    Channel * channel = (Channel *) connection->GetTarget(connection);
    channel->SetDefinedDuringInit(channel);
}


static int ConnectionIsActiveDependency(Connection * conn) {
    return conn->data->isActiveDependency;
}

static void ConnectionSetActiveDependency(Connection * conn, int active) {
    conn->data->isActiveDependency = active;
}

static void ConnectionUpdateFromInput(Connection * connection, TimeInterval * time) {
}

static McxStatus ConnectionUpdateInitialValue(Connection * connection) {
    ConnectionInfo * info = connection->GetInfo(connection);

    Channel * in = (Channel *) connection->data->in;
    Channel * out = (Channel *) connection->data->out;

    ChannelInfo * inInfo = in->GetInfo(in);
    ChannelInfo * outInfo = out->GetInfo(out);

    if (connection->data->state != InInitializationMode) {
        char * buffer = info->ConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Update initial value: Cannot update initial value outside of initialization mode", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    if (!out || !in) {
        char * buffer = info->ConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Update initial value: Cannot update initial value for unconnected connection", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    if (inInfo->GetInitialValue(inInfo)) {
        McxStatus retVal = RETURN_OK;
        ChannelValue * store = &connection->data->store;
        ChannelValue * inChannelValue = inInfo->GetInitialValue(inInfo);
        ChannelValue * inValue = ChannelValueClone(inChannelValue);

        if (NULL == inValue) {
            mcx_log(LOG_ERROR, "Could not clone initial value for initial connection");
            return RETURN_ERROR;
        }

        // The type of the stored value of a connection is the type of the out channel.
        // If the value is taken from the in channel, the value must be converted.
        // TODO: It might be a better idea to use the type of the in channel as type of the connection.
        // Such a change might be more complex to implement.
        if (inValue->type != store->type) {
            TypeConversion * typeConv = (TypeConversion *) object_create(TypeConversion);
            Conversion * conv = (Conversion *) typeConv;
            retVal = typeConv->Setup(typeConv, inValue->type, store->type);
            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "Could not set up initial type conversion");
                object_destroy(typeConv);
                mcx_free(inValue);
                return RETURN_ERROR;
            }
            retVal = conv->convert(conv, inValue);
            object_destroy(typeConv);

            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "Could not convert type of initial value");
                mcx_free(inValue);
                return RETURN_ERROR;
            }
        }

        retVal = ChannelValueSet(store, inValue);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Could not set up initial value in connection");
            mcx_free(inValue);
            return RETURN_ERROR;
        }
        mcx_free(inValue);

        connection->data->useInitialValue = TRUE;
    } else if (outInfo->GetInitialValue(outInfo)) {
        ChannelValueSet(&connection->data->store, outInfo->GetInitialValue(outInfo));
        connection->data->useInitialValue = TRUE;
    } else {
        {
            char * buffer = info->ConnectionString(info);
            mcx_log(LOG_WARNING, "Connection %s: No initial values are specified for the ports of the connection", buffer);
            mcx_free(buffer);
        }
        ChannelValueInit(&connection->data->store, info->GetType(info));
    }

    return RETURN_OK;
}

static void ConnectionInitUpdateFrom(Connection * connection, TimeInterval * time) {
#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        Channel * channel = (Channel *) connection->data->out;
        ChannelInfo * info = channel->GetInfo(channel);
        MCX_DEBUG_LOG("[%f] CONN   (%s) UpdateFromInput", time->startTime, info->GetName(info));
    }
#endif
    // Do nothing
}

static void ConnectionInitUpdateTo(Connection * connection, TimeInterval * time) {
    Channel * channel = (Channel *) connection->data->out;

#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        ChannelInfo * info = channel->GetInfo(channel);
        MCX_DEBUG_LOG("[%f] CONN   (%s) UpdateToOutput", time->startTime, info->GetName(info));
    }
#endif

    if (!connection->data->useInitialValue) {
        ChannelValueSetFromReference(&connection->data->store, channel->GetValueReference(channel));
        if (channel->IsDefinedDuringInit(channel)) {
            connection->SetDefinedDuringInit(connection);
        }
    } else {
        connection->SetDefinedDuringInit(connection);
    }
}

static McxStatus ConnectionEnterInitializationMode(Connection * connection) {
#ifdef MCX_DEBUG
        Channel * channel = (Channel *) connection->data->out;
        ChannelInfo * info = channel->GetInfo(channel);
        MCX_DEBUG_LOG("[%f] CONN   (%s) EnterInit", 0.0, info->GetName(info));
#endif

    if (connection->data->state == InInitializationMode) {
        mcx_log(LOG_ERROR, "Connection: Enter initialization mode: Called multiple times");
        return RETURN_ERROR;
    }

    connection->data->state = InInitializationMode;

    // save functions for normal mode
    connection->data->NormalUpdateFrom = connection->UpdateFromInput;
    connection->data->NormalUpdateTo = connection->UpdateToOutput;
    connection->data->normalValue = connection->data->value;

    // set functions for initialization mode
    connection->UpdateFromInput = ConnectionInitUpdateFrom;
    connection->UpdateToOutput = ConnectionInitUpdateTo;
    connection->data->value = ChannelValueReference(&connection->data->store);
    connection->IsDefinedDuringInit = ConnectionIsDefinedDuringInit;
    connection->SetDefinedDuringInit = ConnectionSetDefinedDuringInit;

    return RETURN_OK;
}

static McxStatus ConnectionExitInitializationMode(Connection * connection, double time) {
    TimeInterval interval = {time, time};

        McxStatus retVal = RETURN_OK;

#ifdef MCX_DEBUG
    if (time < MCX_DEBUG_LOG_TIME) {
        Channel * channel = (Channel *) connection->data->out;
        ChannelInfo * info = channel->GetInfo(channel);
        MCX_DEBUG_LOG("[%f] CONN   (%s) ExitInit", time, info->GetName(info));
    }
#endif

    if (connection->data->state != InInitializationMode) {
        mcx_log(LOG_ERROR, "Connection: Exit initialization mode: Called multiple times");
        return RETURN_ERROR;
    }

    // restore functions for normal mode
    connection->UpdateFromInput = connection->data->NormalUpdateFrom;
    connection->UpdateToOutput = connection->data->NormalUpdateTo;
    connection->data->value = connection->data->normalValue;
    connection->IsDefinedDuringInit = NULL;
    connection->SetDefinedDuringInit(connection); // After initialization all values are defined
    connection->SetDefinedDuringInit = NULL;

    connection->UpdateFromInput(connection, &interval);
    retVal = connection->EnterCommunicationMode(connection, time);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Connection: Exit initialization mode: Cannot enter communication mode");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ConnectionEnterCommunicationMode(Connection * connection, double time) {
    connection->data->state = InCommunicationMode;

    return RETURN_OK;
}

static McxStatus ConnectionEnterCouplingStepMode(Connection * connection
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize)
{
    connection->data->state = InCouplingStepMode;

    return RETURN_OK;
}

McxStatus ConnectionSetup(Connection * connection, ChannelOut * out, ChannelIn * in, ConnectionInfo * info) {
    McxStatus retVal = RETURN_OK;

    Channel * chOut = (Channel *) out;
    ChannelInfo * outInfo = chOut->GetInfo(chOut);

    connection->data->out  = out;
    connection->data->in   = in;
    connection->data->info = info;

    if (in->IsDiscrete(in)) {
        info->SetDiscreteTarget(info);
    }

    ChannelValueInit(&connection->data->store, outInfo->GetType(outInfo));

    // Add connection to channel out
    retVal = out->RegisterConnection(out, connection);
    if (RETURN_OK != retVal) {
        char * buffer = info->ConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Setup connection: Could not register with outport", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    retVal = in->SetConnection(in, connection, outInfo->GetUnit(outInfo), outInfo->GetType(outInfo));
    if (RETURN_OK != retVal) {
        char * buffer = info->ConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Setup connection: Could not register with inport", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static Connection * ConnectionCreate(Connection * connection) {
    connection->data = (ConnectionData *) object_create(ConnectionData);

    if (!connection->data) {
        return NULL;
    }

    connection->Setup = NULL;

    connection->GetSource = ConnectionGetSource;
    connection->GetTarget = ConnectionGetTarget;

    connection->GetValueReference = ConnectionGetValueReference;

    connection->GetInfo   = ConnectionGetInfo;

    connection->IsDecoupled = ConnectionIsDecoupled;

    connection->IsDefinedDuringInit = NULL;
    connection->SetDefinedDuringInit = ConnectionSetDefinedDuringInit;

    connection->IsActiveDependency = ConnectionIsActiveDependency;
    connection->SetActiveDependency = ConnectionSetActiveDependency;

    connection->UpdateFromInput = ConnectionUpdateFromInput;
    connection->UpdateToOutput = NULL;
    connection->UpdateInitialValue = ConnectionUpdateInitialValue;

    connection->EnterCommunicationMode = ConnectionEnterCommunicationMode;
    connection->EnterCouplingStepMode     = ConnectionEnterCouplingStepMode;
    connection->EnterInitializationMode = ConnectionEnterInitializationMode;
    connection->ExitInitializationMode = ConnectionExitInitializationMode;

    connection->AddFilter = NULL;

    return connection;
}

OBJECT_CLASS(Connection, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */