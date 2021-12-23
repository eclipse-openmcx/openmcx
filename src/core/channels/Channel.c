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
#include "core/Config.h"
#include "core/connections/Connection.h"
#include "core/Conversion.h"

#include "core/channels/ChannelInfo.h"
#include "core/channels/Channel.h"
#include "core/channels/Channel_impl.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Channel

static ChannelData * ChannelDataCreate(ChannelData * data) {
    /* create dummy info*/
    data->info = (ChannelInfo *) object_create(ChannelInfo);
    if (!data->info) {
        return NULL;
    }

    data->isDefinedDuringInit = FALSE;
    data->internalValue = NULL;
    ChannelValueInit(&data->value, CHANNEL_UNKNOWN);

    return data;
}

static void ChannelDataDestructor(ChannelData * data) {
    // Note: This is done in Databus
    // object_destroy(data->info);

    ChannelValueDestructor(&data->value);
}

OBJECT_CLASS(ChannelData, Object);


static int ChannelIsDefinedDuringInit(Channel * channel) {
    return channel->data->isDefinedDuringInit;
}

static void ChannelSetDefinedDuringInit(Channel * channel) {
    channel->data->isDefinedDuringInit = TRUE;
}

static ChannelInfo * ChannelGetInfo(Channel * channel) {
    return channel->data->info;
}

static McxStatus ChannelSetup(Channel * channel, ChannelInfo * info) {
    if (channel->data->info) {
        object_destroy(channel->data->info);
    }
    channel->data->info = info;
    info->channel = channel;

    return RETURN_OK;
}

static void ChannelDestructor(Channel * channel) {
    object_destroy(channel->data);
}

static Channel * ChannelCreate(Channel * channel) {
    channel->data = (ChannelData *) object_create(ChannelData);
    if (!channel->data) {
        return NULL;
    }

    channel->GetInfo = ChannelGetInfo;
    channel->Setup = ChannelSetup;
    channel->IsDefinedDuringInit = ChannelIsDefinedDuringInit;
    channel->SetDefinedDuringInit = ChannelSetDefinedDuringInit;

    // virtual functions
    channel->GetValueReference = NULL;
    channel->Update = NULL;
    channel->IsValid = NULL;

    channel->IsConnected = NULL;

    return channel;
}


// ----------------------------------------------------------------------
// ChannelIn

// object that is stored in target component that stores
// the channel connection


static ChannelInData * ChannelInDataCreate(ChannelInData * data) {
    data->connection = NULL;
    data->reference  = NULL;

    data->unitConversion = NULL;
    data->typeConversion = NULL;
    data->linearConversion = NULL;
    data->rangeConversion = NULL;

    data->isDiscrete = FALSE;

    return data;
}

static void ChannelInDataDestructor(ChannelInData * data) {
    if (data->unitConversion) {
        object_destroy(data->unitConversion);
    }
    if (data->typeConversion) {
        object_destroy(data->typeConversion);
    }
    if (data->linearConversion) {
        object_destroy(data->linearConversion);
    }
    if (data->rangeConversion) {
        object_destroy(data->rangeConversion);
    }
}

OBJECT_CLASS(ChannelInData, Object);



static McxStatus ChannelInSetReference(ChannelIn * in, void * reference, ChannelType type) {
    Channel * ch = (Channel *) in;
    ChannelInfo * info = ch->GetInfo(ch);

    if (!in) {
        mcx_log(LOG_ERROR, "Port: Set inport reference: Invalid port");
        return RETURN_ERROR;
    }
    if (in->data->reference) {
        mcx_log(LOG_ERROR, "Port %s: Set inport reference: Reference already set", info->GetLogName(info));
        return RETURN_ERROR;
    }

    if (CHANNEL_UNKNOWN != type) {
        if (!info) {
            mcx_log(LOG_ERROR, "Port %s: Set inport reference: Port not set up", info->GetLogName(info));
            return RETURN_ERROR;
        }
        if (info->GetType(info) != type) {
            if (info->IsBinary(info) && (type == CHANNEL_BINARY || type == CHANNEL_BINARY_REFERENCE)) {
                // ok
            } else {
                mcx_log(LOG_ERROR, "Port %s: Set inport reference: Mismatching types", info->GetLogName(info));
                return RETURN_ERROR;
            }
        }
    }

    in->data->reference = reference;

    return RETURN_OK;
}

static const void * ChannelInGetValueReference(Channel * channel) {
    ChannelIn * in = (ChannelIn *) channel;
    if (!channel->IsValid(channel)) {
        const static int maxCountError = 10;
        static int i = 0;
        if (i < maxCountError) {
            ChannelInfo * info = channel->GetInfo(channel);
            i++;
            mcx_log(LOG_ERROR, "Port %s: Get value reference: No value reference for inport", info->GetLogName(info));
            if (i == maxCountError) {
                mcx_log(LOG_ERROR, "Port %s: Get value reference: No value reference for inport - truncated", info->GetLogName(info)) ;
            }
        }
        return NULL;
    }

    return ChannelValueReference(&channel->data->value);
}

static McxStatus ChannelInUpdate(Channel * channel, TimeInterval * time) {
    ChannelIn * in = (ChannelIn *) channel;
    ChannelInfo * info = channel->GetInfo(channel);
    Connection * conn = in->data->connection;

    McxStatus retVal = RETURN_OK;

    /* if no connection is present we have nothing to update*/
    if (conn) {
        ConnectionInfo * connInfo = NULL;
        ChannelValue * val = &channel->data->value;

        connInfo = conn->GetInfo(conn);

        ChannelValueDestructor(val);
        ChannelValueInit(val, connInfo->GetType(connInfo));

        /* Update the connection for the current time */
        conn->UpdateToOutput(conn, time);
        ChannelValueSetFromReference(val, conn->GetValueReference(conn));

        //type
        if (in->data->typeConversion) {
            Conversion * conversion = (Conversion *) in->data->typeConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute type conversion", info->GetLogName(info));
                return RETURN_ERROR;
            }
        }


        if (info->GetType(info) == CHANNEL_DOUBLE) {
            ChannelValue * val = &channel->data->value;
            // unit
            if (in->data->unitConversion) {
                Conversion * conversion = (Conversion *) in->data->unitConversion;
                retVal = conversion->convert(conversion, val);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute unit conversion", info->GetLogName(info));
                    return RETURN_ERROR;
                }
            }
        }

        if (info->GetType(info) == CHANNEL_DOUBLE || info->GetType(info) == CHANNEL_INTEGER) {
            ChannelValue * val = &channel->data->value;

            // linear
            if (in->data->linearConversion) {
                Conversion * conversion = (Conversion *) in->data->linearConversion;
                retVal = conversion->convert(conversion, val);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute linear conversion", info->GetLogName(info));
                    return RETURN_ERROR;
                }
            }

            // range
            if (in->data->rangeConversion) {
                Conversion * conversion = (Conversion *) in->data->rangeConversion;
                retVal = conversion->convert(conversion, val);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute range conversion", info->GetLogName(info));
                    return RETURN_ERROR;
                }
            }
        }
    }

    /* no reference from the component was set, skip updating*/
    if (!in->data->reference || !channel->GetValueReference(channel)) {
        return RETURN_OK;
    }


    switch (info->GetType(info)) {
    case CHANNEL_DOUBLE:
#ifdef MCX_DEBUG
        if (time->startTime < MCX_DEBUG_LOG_TIME) {
            MCX_DEBUG_LOG("[%f] CH IN  (%s) (%f, %f)", time->startTime, info->GetLogName(info), time->startTime, * (double *) channel->GetValueReference(channel));
        }
#endif // MCX_DEBUG
        * (double *) in->data->reference = * (double *) channel->GetValueReference(channel);
        break;
    case CHANNEL_INTEGER:
        * (int *) in->data->reference = * (int *) channel->GetValueReference(channel);
        break;
    case CHANNEL_BOOL:
        * (int *) in->data->reference = * (int *) channel->GetValueReference(channel);
        break;
    case CHANNEL_STRING:
    {
        const void * reference = channel->GetValueReference(channel);

        if (NULL != reference && NULL != * (const char * *) reference ) {
            if (* (char * *) in->data->reference) {
                mcx_free(* (char * *) in->data->reference);
            }
            * (char * *) in->data->reference = (char *) mcx_calloc(strlen(* (const char **) reference) + 1, sizeof(char));
            if (* (char * *) in->data->reference) {
                strncpy(* (char * *) in->data->reference, * (const char **)reference, strlen(* (const char **)reference) + 1);
            }
        }
        break;
    }
    case CHANNEL_BINARY:
    {
        const void * reference = channel->GetValueReference(channel);

        if (NULL != reference && NULL != ((const binary_string *) reference)->data) {
            if (((binary_string *) in->data->reference)->data) {
                mcx_free(((binary_string *) in->data->reference)->data);
            }
            ((binary_string *) in->data->reference)->len = ((const binary_string *) reference)->len;
            ((binary_string *) in->data->reference)->data = (char *) mcx_malloc(((binary_string *) in->data->reference)->len);
            if (((binary_string *) in->data->reference)->data) {
                memcpy(((binary_string *) in->data->reference)->data, ((const binary_string *) reference)->data, ((binary_string *) in->data->reference)->len);
            }
        }
        break;
    }
    case CHANNEL_BINARY_REFERENCE:
    {
        const void * reference = channel->GetValueReference(channel);

        if (NULL != reference && NULL != ((binary_string *) reference)->data) {
            ((binary_string *) in->data->reference)->len = ((binary_string *) reference)->len;
            ((binary_string *) in->data->reference)->data = ((binary_string *) reference)->data;
        }
        break;
    }
    default:
        break;
    }

    return RETURN_OK;
}

static int ChannelInIsValid(Channel * channel) {

    if (channel->IsConnected(channel)) {
        return TRUE;
    } else {
        ChannelInfo * info = channel->GetInfo(channel);
        if (info && NULL != info->defaultValue) {
            return TRUE;
        }
    }
    return FALSE;
}

static void ChannelInSetDiscrete(ChannelIn * in) {
    in->data->isDiscrete = TRUE;
}

static int ChannelInIsDiscrete(ChannelIn * in) {
    return in->data->isDiscrete;
}

static int ChannelInIsConnected(Channel * channel) {
    if (channel->data->info && channel->data->info->connected) {
        return TRUE;
    } else {
        ChannelIn * in = (ChannelIn *) channel;
        if (NULL != in->data->connection) {
            return TRUE;
        }
    }

    return FALSE;
}

static ConnectionInfo * ChannelInGetConnectionInfo(ChannelIn * in) {
    if (in->data->connection) {
        return in->data->connection->GetInfo(in->data->connection);
    } else {
        return NULL;
    }
}

static Connection * ChannelInGetConnection(ChannelIn * in) {
    if (in->data->connection) {
        return in->data->connection;
    } else {
        return NULL;
    }
}

static McxStatus ChannelInSetConnection(ChannelIn * in, Connection * connection, const char * unit, ChannelType type
) {
    Channel * channel = (Channel *) in;
    ChannelInfo * inInfo = NULL;

    McxStatus retVal;

    in->data->connection = connection;
    channel->data->internalValue = connection->GetValueReference(connection);

    // setup unit conversion
    inInfo = channel->GetInfo(channel);

    if (inInfo->GetType(inInfo) == CHANNEL_DOUBLE) {
        in->data->unitConversion = (UnitConversion *) object_create(UnitConversion);
        retVal = in->data->unitConversion->Setup(in->data->unitConversion,
                                                 unit,
                                                 inInfo->GetUnit(inInfo));
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Set inport connection: Could not setup unit conversion", inInfo->GetLogName(inInfo));
            return RETURN_ERROR;
        }

        if (in->data->unitConversion->IsEmpty(in->data->unitConversion)) {
            object_destroy(in->data->unitConversion);
        }
    }

    // setup type conversion
    if (inInfo->GetType(inInfo) != type) {
        in->data->typeConversion = (TypeConversion *) object_create(TypeConversion);
        retVal = in->data->typeConversion->Setup(in->data->typeConversion,
                                                 type,
                                                 inInfo->GetType(inInfo));
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Set connection: Could not setup type conversion", inInfo->GetLogName(inInfo));
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;

}

static McxStatus ChannelInSetup(ChannelIn * in, ChannelInfo * info) {
    Channel * channel = (Channel *) in;
    McxStatus retVal;

    retVal = channel->Setup(channel, info); // call base-class function

    // types
    if (info->type == CHANNEL_UNKNOWN) {
        mcx_log(LOG_ERROR, "Port %s: Setup inport: Unknown type", info->GetLogName(info));
        return RETURN_ERROR;
    }
    ChannelValueInit(&channel->data->value, info->type);

    // default value
    if (info->defaultValue) {
        ChannelValueSet(&channel->data->value, info->defaultValue);

        // apply range and linear conversions immediately
        retVal = ConvertRange(info->GetMin(info), info->GetMax(info), &channel->data->value);
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
        }

        retVal = ConvertLinear(info->GetScale(info), info->GetOffset(info), &channel->data->value);
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
        }

        channel->SetDefinedDuringInit(channel);
        channel->data->internalValue = ChannelValueReference(&channel->data->value);
    }

    // unit conversion is setup when a connection is set

    // min/max conversions are only used for double types
    if (info->GetType(info) == CHANNEL_DOUBLE
        || info->GetType(info) == CHANNEL_INTEGER)
    {
        ChannelValue * min = info->GetMin(info);
        ChannelValue * max = info->GetMax(info);

        ChannelValue * scale  = info->GetScale(info);
        ChannelValue * offset = info->GetOffset(info);

        in->data->rangeConversion = (RangeConversion *) object_create(RangeConversion);
        retVal = in->data->rangeConversion->Setup(in->data->rangeConversion, min, max);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup inport: Could not setup range conversion", info->GetLogName(info));
            object_destroy(in->data->rangeConversion);
            return RETURN_ERROR;
        } else {
            if (in->data->rangeConversion->IsEmpty(in->data->rangeConversion)) {
                object_destroy(in->data->rangeConversion);
            }
        }

        in->data->linearConversion = (LinearConversion *) object_create(LinearConversion);
        retVal = in->data->linearConversion->Setup(in->data->linearConversion, scale, offset);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup inport: Could not setup linear conversion", info->GetLogName(info));
            object_destroy(in->data->linearConversion);
            return RETURN_ERROR;
        } else {
            if (in->data->linearConversion->IsEmpty(in->data->linearConversion)) {
                object_destroy(in->data->linearConversion);
            }
        }
    }

    return RETURN_OK;
}

static void ChannelInDestructor(ChannelIn * in) {
    object_destroy(in->data);
}

static ChannelIn * ChannelInCreate(ChannelIn * in) {
    Channel * channel = (Channel *) in;

    in->data = (ChannelInData *) object_create(ChannelInData);
    if (!in->data) {
        return NULL;
    }

    // virtual functions
    channel->GetValueReference = ChannelInGetValueReference;
    channel->IsValid           = ChannelInIsValid;
    channel->Update            = ChannelInUpdate;
    channel->IsConnected       = ChannelInIsConnected;

    in->Setup        = ChannelInSetup;
    in->SetReference = ChannelInSetReference;

    in->GetConnectionInfo = ChannelInGetConnectionInfo;

    in->GetConnection = ChannelInGetConnection;
    in->SetConnection = ChannelInSetConnection;

    in->IsDiscrete = ChannelInIsDiscrete;
    in->SetDiscrete = ChannelInSetDiscrete;

    return in;
}


// ----------------------------------------------------------------------
// ChannelOut

static ChannelOutData * ChannelOutDataCreate(ChannelOutData * data) {
    data->valueFunction = NULL;
    data->rangeConversion = NULL;
    data->linearConversion = NULL;

    data->rangeConversionIsActive = TRUE;

    data->connections = (ObjectList *) object_create(ObjectList);

    data->nanCheck = NAN_CHECK_ALWAYS;

    data->countNaNCheckWarning = 0;
    data->maxNumNaNCheckWarning = 0;


    return data;
}

static void ChannelOutDataDestructor(ChannelOutData * data) {
    ObjectList * conns = data->connections;
    size_t i = 0;

    if (data->rangeConversion) {
        object_destroy(data->rangeConversion);
    }
    if (data->linearConversion) {
        object_destroy(data->linearConversion);
    }

    for (i = 0; i < conns->Size(conns); i++) {
        object_destroy(conns->elements[i]);
    }
    object_destroy(data->connections);
}

OBJECT_CLASS(ChannelOutData, Object);



static McxStatus ChannelOutSetup(ChannelOut * out, ChannelInfo * info, Config * config) {
    Channel * channel = (Channel *) out;

    ChannelValue * min = info->GetMin(info);
    ChannelValue * max = info->GetMax(info);

    ChannelValue * scale  = info->GetScale(info);
    ChannelValue * offset = info->GetOffset(info);

    McxStatus retVal;

    retVal = channel->Setup(channel, info); // call base-class function

    // default value
    if (info->type == CHANNEL_UNKNOWN) {
        mcx_log(LOG_ERROR, "Port %s: Setup outport: Unknown type", info->GetLogName(info));
        return RETURN_ERROR;
    }
    ChannelValueInit(&channel->data->value, info->type);

    // default value
    if (info->defaultValue) {
        channel->data->internalValue = ChannelValueReference(info->defaultValue);
    }


    // min/max conversions are only used for double types
    if (info->GetType(info) == CHANNEL_DOUBLE
        || info->GetType(info) == CHANNEL_INTEGER)
    {
        out->data->rangeConversion = (RangeConversion *) object_create(RangeConversion);
        retVal = out->data->rangeConversion->Setup(out->data->rangeConversion, min, max);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup outport: Could not setup range conversion", info->GetLogName(info));
            object_destroy(out->data->rangeConversion);
            return RETURN_ERROR;
        } else {
            if (out->data->rangeConversion->IsEmpty(out->data->rangeConversion)) {
                object_destroy(out->data->rangeConversion);
            }
        }

        out->data->linearConversion = (LinearConversion *) object_create(LinearConversion);
        retVal = out->data->linearConversion->Setup(out->data->linearConversion, scale, offset);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup outport: Could not setup linear conversion", info->GetLogName(info));
            object_destroy(out->data->linearConversion);
            return RETURN_ERROR;
        } else {
            if (out->data->linearConversion->IsEmpty(out->data->linearConversion)) {
                object_destroy(out->data->linearConversion);
            }
        }
    }

    if (!config) {
        mcx_log(LOG_DEBUG, "Port %s: Setup outport: No config available", info->GetLogName(info));
        return RETURN_ERROR;
    }

    out->data->nanCheck = config->nanCheck;
    out->data->maxNumNaNCheckWarning = config->nanCheckNumMessages;

    return RETURN_OK;
}

static McxStatus ChannelOutRegisterConnection(ChannelOut * out, Connection * connection) {
    ObjectList * conns = out->data->connections;

    return conns->PushBack(conns, (Object *) connection);
}

static const void * ChannelOutGetValueReference(Channel * channel) {
    ChannelOut * out = (ChannelOut *) channel;
    ChannelInfo * info = channel->GetInfo(channel);

    // check if out is initialized
    if (!channel->IsValid(channel)) {
        mcx_log(LOG_ERROR, "Port %s: Get value reference: No Value Reference", info->GetLogName(info));
        return NULL;
    }

    return ChannelValueReference(&channel->data->value);
}

static const proc * ChannelOutGetFunction(ChannelOut * out) {
    return out->data->valueFunction;
}

static ObjectList * ChannelOutGetConnections(ChannelOut * out) {
    return out->data->connections;
}

static int ChannelOutIsValid(Channel * channel) {
    return (NULL != channel->data->internalValue);
}

static int ChannelOutIsConnected(Channel * channel) {
    if (channel->data->info->connected) {
        return TRUE;
    } else {
        ChannelOut * out = (ChannelOut *) channel;
        if (NULL != out->data->connections) {
            if (out->data->connections->Size(out->data->connections)) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static McxStatus ChannelOutSetReference(ChannelOut * out, const void * reference, ChannelType type) {
    Channel * channel = (Channel *) out;
    ChannelInfo * info = NULL;

    if (!out) {
        mcx_log(LOG_ERROR, "Port: Set outport reference: Invalid port");
        return RETURN_ERROR;
    }
    info = channel->GetInfo(channel);
    if (!info) {
        mcx_log(LOG_ERROR, "Port %s: Set outport reference: Port not set up", info->GetLogName(info));
        return RETURN_ERROR;
    }
    if (channel->data->internalValue
        && !(info->defaultValue && channel->data->internalValue == ChannelValueReference(info->defaultValue))) {
        mcx_log(LOG_ERROR, "Port %s: Set outport reference: Reference already set", info->GetLogName(info));
        return RETURN_ERROR;
    }
    if (CHANNEL_UNKNOWN != type) {
        if (info->GetType(info) != type) {
            if (info->IsBinary(info) && (type == CHANNEL_BINARY || type == CHANNEL_BINARY_REFERENCE)) {
                // ok
            } else {
                mcx_log(LOG_ERROR, "Port %s: Set outport reference: Mismatching types", info->GetLogName(info));
                return RETURN_ERROR;
            }
        }
    }

    channel->data->internalValue = reference;

    return RETURN_OK;
}

static McxStatus ChannelOutSetReferenceFunction(ChannelOut * out, const proc * reference, ChannelType type) {
    Channel * channel = (Channel *) out;
    ChannelInfo * info = NULL;
    if (!out) {
        mcx_log(LOG_ERROR, "Port: Set outport function: Invalid port");
        return RETURN_ERROR;
    }

    info = channel->GetInfo(channel);
    if (CHANNEL_UNKNOWN != type) {
        if (info->type != type) {
            mcx_log(LOG_ERROR, "Port %s: Set outport function: Mismatching types", info->GetLogName(info));
            return RETURN_ERROR;
        }
    }

    if (out->data->valueFunction) {
        mcx_log(LOG_ERROR, "Port %s: Set outport function: Reference already set", info->GetLogName(info));
        return RETURN_ERROR;
    }

    // Save channel procedure
    out->data->valueFunction = (const proc *) reference;

    // Setup value reference to point to internal value
    channel->data->internalValue = ChannelValueReference(&channel->data->value);

    return RETURN_OK;
}

static void WarnAboutNaN(LogSeverity level, ChannelInfo * info, TimeInterval * time, size_t * count, size_t * max) {
    if (*max > 0) {
        if (*count < *max) {
            mcx_log(level, "Outport %s at time %f is not a number (NaN)",
                   info->GetName(info), time->startTime);
            *count += 1;
            if (*count == *max) {
                mcx_log(level, "This warning will not be shown anymore");
            }
        }
    } else {
        mcx_log(level, "Outport %s at time %f is not a number (NaN)",
               info->GetName(info), time->startTime);
    }
}

static McxStatus ChannelOutUpdate(Channel * channel, TimeInterval * time) {
    ChannelOut * out = (ChannelOut *)channel;
    ChannelInfo * info = ((Channel *)out)->GetInfo((Channel *)out);

    ObjectList * conns = out->data->connections;

    McxStatus retVal = RETURN_OK;

    time->endTime = time->startTime;
    {
        size_t j = 0;

        // Set Value
        if (out->GetFunction(out)) {
            // function value
            proc * p = (proc *) out->GetFunction(out);
            double val = p->fn(time, p->env);
#ifdef MCX_DEBUG
            if (time->startTime < MCX_DEBUG_LOG_TIME) {
                MCX_DEBUG_LOG("[%f] CH OUT (%s) (%f, %f)", time->startTime, info->GetLogName(info), time->startTime, val);
            }
#endif // MCX_DEBUG
            ChannelValueSetFromReference(&channel->data->value, &val);
        } else {
#ifdef MCX_DEBUG
            if (time->startTime < MCX_DEBUG_LOG_TIME) {
                if (CHANNEL_DOUBLE == info->GetType(info)) {
                    MCX_DEBUG_LOG("[%f] CH OUT (%s) (%f, %f)",
                                  time->startTime,
                                  info->GetLogName(info),
                                  time->startTime,
                                  * (double *) channel->data->internalValue);
                } else {
                    MCX_DEBUG_LOG("[%f] CH OUT (%s)", time->startTime, info->GetLogName(info));
                }
            }
#endif // MCX_DEBUG
            ChannelValueSetFromReference(&channel->data->value, channel->data->internalValue);
        }

        // Apply conversion
        if (info->GetType(info) == CHANNEL_DOUBLE ||
            info->GetType(info) == CHANNEL_INTEGER) {
            ChannelValue * val = &channel->data->value;

            // range
            if (out->data->rangeConversion) {
                if (out->data->rangeConversionIsActive) {
                    Conversion * conversion = (Conversion *) out->data->rangeConversion;
                    retVal = conversion->convert(conversion, val);
                    if (RETURN_OK != retVal) {
                        mcx_log(LOG_ERROR, "Port %s: Update outport: Could not execute range conversion", info->GetLogName(info));
                        return RETURN_ERROR;
                    }
                }
            }

            // linear
            if (out->data->linearConversion) {
                Conversion * conversion = (Conversion *) out->data->linearConversion;
                retVal = conversion->convert(conversion, val);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "Port %s: Update outport: Could not execute linear conversion", info->GetLogName(info));
                    return RETURN_ERROR;
                }
            }
        }

        // Notify connections of new values
        size_t connSize = conns->Size(conns);
        for (j = 0; j < connSize; j++) {
            Connection * connection = (Connection *) conns->At(conns, j);
            channel->SetDefinedDuringInit(channel);
            connection->UpdateFromInput(connection, time);
        }
    }


    if (CHANNEL_DOUBLE == info->GetType(info)) {
        const double * val = NULL;

        {
            val = &channel->data->value.value.d;
        }

        if (isnan(*val))
        {
            switch (out->data->nanCheck) {

            case NAN_CHECK_ALWAYS:
                mcx_log(LOG_ERROR, "Outport %s at time %f is not a number (NaN)",
                       info->GetName(info), time->startTime);
                return RETURN_ERROR;

            case NAN_CHECK_CONNECTED:
                if (conns->Size(conns) > 0) {
                    mcx_log(LOG_ERROR, "Outport %s at time %f is not a number (NaN)",
                           info->GetName(info), time->startTime);
                    return RETURN_ERROR;
                } else {
                    WarnAboutNaN(LOG_WARNING, info, time, &out->data->countNaNCheckWarning, &out->data->maxNumNaNCheckWarning);
                    break;
                }

            case NAN_CHECK_NEVER:
                WarnAboutNaN((conns->Size(conns) > 0) ? LOG_ERROR : LOG_WARNING,
                             info, time, &out->data->countNaNCheckWarning, &out->data->maxNumNaNCheckWarning);
                break;
            }
        }
    }


    return RETURN_OK;
}

static void ChannelOutDestructor(ChannelOut * out) {
    object_destroy(out->data);
}

static ChannelOut * ChannelOutCreate(ChannelOut * out) {
    Channel * channel = (Channel *) out;

    out->data = (ChannelOutData *) object_create(ChannelOutData);
    if (!out->data) {
        return NULL;
    }

    // virtual functions
    channel->GetValueReference = ChannelOutGetValueReference;
    channel->IsValid           = ChannelOutIsValid;
    channel->Update            = ChannelOutUpdate;
    channel->IsConnected       = ChannelOutIsConnected;

    out->Setup                = ChannelOutSetup;
    out->RegisterConnection   = ChannelOutRegisterConnection;
    out->SetReference         = ChannelOutSetReference;
    out->SetReferenceFunction = ChannelOutSetReferenceFunction;

    out->GetFunction = ChannelOutGetFunction;
    out->GetConnections = ChannelOutGetConnections;

    return out;
}


// ----------------------------------------------------------------------
// ChannelLocal

static void ChannelLocalDataDestructor(ChannelLocalData * data) {
}

static ChannelLocalData * ChannelLocalDataCreate(ChannelLocalData * data) {
    return data;
}

OBJECT_CLASS(ChannelLocalData, Object);

static McxStatus ChannelLocalSetup(ChannelLocal * local, ChannelInfo * info) {
    Channel * channel = (Channel *) local;

    return channel->Setup(channel, info);
}

static const void * ChannelLocalGetValueReference(Channel * channel) {
    return channel->data->internalValue;
}

static McxStatus ChannelLocalUpdate(Channel * channel, TimeInterval * time) {
    return RETURN_OK;
}

static int ChannelLocalIsValid(Channel * channel) {
    return (channel->data->internalValue != NULL);
}

// TODO: Unify with ChannelOutsetReference (similar code)
static McxStatus ChannelLocalSetReference(ChannelLocal * local,
                                          const void * reference,
                                          ChannelType type) {
    Channel * channel = (Channel *) local;
    ChannelInfo * info = NULL;

    info = channel->GetInfo(channel);
    if (!info) {
        mcx_log(LOG_ERROR, "Port: Set local value reference: Port not set up");
        return RETURN_ERROR;
    }
    if (channel->data->internalValue
        && !(info->defaultValue && channel->data->internalValue == ChannelValueReference(info->defaultValue))) {
        mcx_log(LOG_ERROR, "Port %s: Set local value reference: Reference already set", info->GetLogName(info));
        return RETURN_ERROR;
    }
    if (CHANNEL_UNKNOWN != type) {
        if (info->GetType(info) != type) {
            mcx_log(LOG_ERROR, "Port %s: Set local value reference: Mismatching types", info->GetLogName(info));
            return RETURN_ERROR;
        }
    }

    channel->data->internalValue = reference;

    return RETURN_OK;
}

static void ChannelLocalDestructor(ChannelLocal * local) {
    object_destroy(local->data);
}

static ChannelLocal * ChannelLocalCreate(ChannelLocal * local) {
    Channel * channel = (Channel *) local;

    local->data = (ChannelLocalData *) object_create(ChannelLocalData);
    if (!local->data) {
        return NULL;
    }

    // virtual functions
    channel->GetValueReference = ChannelLocalGetValueReference;
    channel->Update            = ChannelLocalUpdate;
    channel->IsValid           = ChannelLocalIsValid;

    channel->IsConnected       = ChannelLocalIsValid;

    local->Setup        = ChannelLocalSetup;
    local->SetReference = ChannelLocalSetReference;

    return local;
}

OBJECT_CLASS(Channel, Object);
OBJECT_CLASS(ChannelIn, Channel);
OBJECT_CLASS(ChannelOut, Channel);
OBJECT_CLASS(ChannelLocal, Channel);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */