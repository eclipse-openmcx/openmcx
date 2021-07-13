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
#include "core/channels/ChannelValue.h"
#include "core/channels/Channel_impl.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Channel


static int ChannelIsDefinedDuringInit(Channel * channel) {
    return channel->isDefinedDuringInit;
}

static void ChannelSetDefinedDuringInit(Channel * channel) {
    channel->isDefinedDuringInit = TRUE;
}

static McxStatus ChannelSetup(Channel * channel, ChannelInfo * info) {
    McxStatus retVal = RETURN_OK;

    info->channel = channel;

    retVal = ChannelInfoSetFrom(&channel->info, info);
    if (RETURN_ERROR == retVal) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void ChannelDestructor(Channel * channel) {
    ChannelInfoDestroy(&channel->info);
    ChannelValueDestructor(&channel->value);
}

static Channel * ChannelCreate(Channel * channel) {
    if (RETURN_ERROR == ChannelInfoInit(&channel->info)) {
        return NULL;
    }

    channel->isDefinedDuringInit = FALSE;
    channel->internalValue = NULL;
    ChannelValueInit(&channel->value, &ChannelTypeUnknown);

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



static McxStatus ChannelInSetReference(ChannelIn * in, void * reference, ChannelType * type) {
    Channel * ch = (Channel *) in;
    ChannelInfo * info = &ch->info;

    if (!in) {
        mcx_log(LOG_ERROR, "Port: Set inport reference: Invalid port");
        return RETURN_ERROR;
    }
    if (in->data->reference) {
        mcx_log(LOG_ERROR, "Port %s: Set inport reference: Reference already set", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    if (ChannelTypeIsValid(type)) {
        if (!info) {
            mcx_log(LOG_ERROR, "Port %s: Set inport reference: Port not set up", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }
        if (!ChannelTypeMatch(info->type, type)) {
            mcx_log(LOG_ERROR, "Port %s: Set inport reference: Mismatching types", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
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
            ChannelInfo * info = &channel->info;
            i++;
            mcx_log(LOG_ERROR, "Port %s: Get value reference: No value reference for inport", ChannelInfoGetLogName(info));
            if (i == maxCountError) {
                mcx_log(LOG_ERROR, "Port %s: Get value reference: No value reference for inport - truncated", ChannelInfoGetLogName(info)) ;
            }
        }
        return NULL;
    }

    return ChannelValueReference(&channel->value);
}

static McxStatus ChannelInUpdate(Channel * channel, TimeInterval * time) {
    ChannelIn * in = (ChannelIn *) channel;
    ChannelInfo * info = &channel->info;
    Connection * conn = in->data->connection;

    McxStatus retVal = RETURN_OK;

    /* if no connection is present we have nothing to update*/
    if (conn) {
        ConnectionInfo * connInfo = NULL;
        ChannelValue * val = &channel->value;

        connInfo = &conn->info;

        ChannelValueDestructor(val);
        ChannelValueInit(val, ConnectionInfoGetType(connInfo));

        /* Update the connection for the current time */
        if (RETURN_OK != conn->UpdateToOutput(conn, time)) {
            mcx_log(LOG_ERROR, "Port %s: Update inport: UpdateToOutput of connection failed", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }
        if (RETURN_OK != ChannelValueSetFromReference(val, conn->GetValueReference(conn))) {
            mcx_log(LOG_ERROR, "Port %s: Update inport: ChannelValueSetFromReference failed", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }

        //type
        if (in->data->typeConversion) {
            Conversion * conversion = (Conversion *) in->data->typeConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute type conversion", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }
    }


    // Conversions
    {
        ChannelValue * val =  &channel->value;


        // unit
        if (in->data->unitConversion) {
            Conversion * conversion = (Conversion *) in->data->unitConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute unit conversion", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }


        // linear
        if (in->data->linearConversion) {
            Conversion * conversion = (Conversion *) in->data->linearConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute linear conversion", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }

        // range
        if (in->data->rangeConversion) {
            Conversion * conversion = (Conversion *) in->data->rangeConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute range conversion", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }
    }

    /* no reference from the component was set, skip updating*/
    if (!in->data->reference || !channel->GetValueReference(channel)) {
        return RETURN_OK;
    }

    if (time->startTime < MCX_DEBUG_LOG_TIME && ChannelTypeEq(info->type, &ChannelTypeDouble)) {
        MCX_DEBUG_LOG("[%f] CH IN  (%s) (%f, %f)", time->startTime, ChannelInfoGetLogName(info), time->startTime, * (double *) channel->GetValueReference(channel));
    }

    if (RETURN_OK != ChannelValueDataSetFromReference(in->data->reference, info->type, channel->GetValueReference(channel))) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static int ChannelInIsValid(Channel * channel) {

    if (channel->IsConnected(channel)) {
        return TRUE;
    } else {
        ChannelInfo * info = &channel->info;
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
    if (ChannelTypeIsValid(channel->info.type) && channel->info.connected) {
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
        return &in->data->connection->info;
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

static McxStatus ChannelInSetConnection(ChannelIn * in, Connection * connection, const char * unit, ChannelType * type) {
    Channel * channel = (Channel *) in;
    ChannelInfo * inInfo = NULL;

    McxStatus retVal;

    in->data->connection = connection;
    channel->internalValue = connection->GetValueReference(connection);

    // setup unit conversion
    inInfo = &channel->info;

    if (ChannelTypeEq(inInfo->type, &ChannelTypeDouble)) {
        in->data->unitConversion = (UnitConversion *) object_create(UnitConversion);
        retVal = in->data->unitConversion->Setup(in->data->unitConversion,
                                                 unit,
                                                 inInfo->unitString);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Set inport connection: Could not setup unit conversion", ChannelInfoGetLogName(inInfo));
            return RETURN_ERROR;
        }

        if (in->data->unitConversion->IsEmpty(in->data->unitConversion)) {
            object_destroy(in->data->unitConversion);
        }
    }

    // setup type conversion
    if (!ChannelTypeEq(inInfo->type, type)) {
        in->data->typeConversion = (TypeConversion *) object_create(TypeConversion);
        retVal = in->data->typeConversion->Setup(in->data->typeConversion,
                                                 type,
                                                 inInfo->type);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Set connection: Could not setup type conversion", ChannelInfoGetLogName(inInfo));
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
    if (!ChannelTypeIsValid(info->type)) {
        mcx_log(LOG_ERROR, "Port %s: Setup inport: Unknown type", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }
    ChannelValueInit(&channel->value, info->type);

    // default value
    if (info->defaultValue) {
        ChannelValueSet(&channel->value, info->defaultValue);

        // apply range and linear conversions immediately
        retVal = ConvertRange(info->min, info->max, &channel->value);
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
        }

        retVal = ConvertLinear(info->scale, info->offset, &channel->value);
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
        }

        channel->SetDefinedDuringInit(channel);
        channel->internalValue = ChannelValueReference(&channel->value);
    }

    // unit conversion is setup when a connection is set

    // min/max conversions are only used for double types
    if (ChannelTypeEq(info->type, &ChannelTypeDouble)
        || ChannelTypeEq(info->type, &ChannelTypeInteger))
    {
        ChannelValue * min = info->min;
        ChannelValue * max = info->max;

        ChannelValue * scale  = info->scale;
        ChannelValue * offset = info->offset;

        in->data->rangeConversion = (RangeConversion *) object_create(RangeConversion);
        retVal = in->data->rangeConversion->Setup(in->data->rangeConversion, min, max);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup inport: Could not setup range conversion", ChannelInfoGetLogName(info));
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
            mcx_log(LOG_ERROR, "Port %s: Setup inport: Could not setup linear conversion", ChannelInfoGetLogName(info));
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

    ChannelValue * min = info->min;
    ChannelValue * max = info->max;

    ChannelValue * scale  = info->scale;
    ChannelValue * offset = info->offset;

    McxStatus retVal;

    retVal = channel->Setup(channel, info); // call base-class function

    // default value
    if (!ChannelTypeIsValid(info->type)) {
        mcx_log(LOG_ERROR, "Port %s: Setup outport: Unknown type", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }
    ChannelValueInit(&channel->value, info->type);

    // default value
    if (info->defaultValue) {
        channel->internalValue = ChannelValueReference(channel->info.defaultValue);
    }


    // min/max conversions are only used for double types
    if (ChannelTypeEq(info->type, &ChannelTypeDouble)
        || ChannelTypeEq(info->type, &ChannelTypeInteger))
    {
        out->data->rangeConversion = (RangeConversion *) object_create(RangeConversion);
        retVal = out->data->rangeConversion->Setup(out->data->rangeConversion, min, max);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup outport: Could not setup range conversion", ChannelInfoGetLogName(info));
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
            mcx_log(LOG_ERROR, "Port %s: Setup outport: Could not setup linear conversion", ChannelInfoGetLogName(info));
            object_destroy(out->data->linearConversion);
            return RETURN_ERROR;
        } else {
            if (out->data->linearConversion->IsEmpty(out->data->linearConversion)) {
                object_destroy(out->data->linearConversion);
            }
        }
    }

    if (!config) {
        mcx_log(LOG_DEBUG, "Port %s: Setup outport: No config available", ChannelInfoGetLogName(info));
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
    ChannelInfo * info = &channel->info;

    // check if out is initialized
    if (!channel->IsValid(channel)) {
        mcx_log(LOG_ERROR, "Port %s: Get value reference: No Value Reference", ChannelInfoGetLogName(info));
        return NULL;
    }

    return ChannelValueReference(&channel->value);
}

static const proc * ChannelOutGetFunction(ChannelOut * out) {
    return out->data->valueFunction;
}

static ObjectList * ChannelOutGetConnections(ChannelOut * out) {
    return out->data->connections;
}

static int ChannelOutIsValid(Channel * channel) {
    return (NULL != channel->internalValue);
}

static int ChannelOutIsConnected(Channel * channel) {
    if (channel->info.connected) {
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

static McxStatus ChannelOutSetReference(ChannelOut * out, const void * reference, ChannelType * type) {
    Channel * channel = (Channel *) out;
    ChannelInfo * info = NULL;

    if (!out) {
        mcx_log(LOG_ERROR, "Port: Set outport reference: Invalid port");
        return RETURN_ERROR;
    }
    info = &channel->info;
    if (!info) {
        mcx_log(LOG_ERROR, "Port %s: Set outport reference: Port not set up", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }
    if (channel->internalValue
        && !(info->defaultValue && channel->internalValue == ChannelValueReference(info->defaultValue))) {
        mcx_log(LOG_ERROR, "Port %s: Set outport reference: Reference already set", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }
    if (ChannelTypeIsValid(type)) {
        if (!ChannelTypeEq(info->type, type)) {
            if (ChannelInfoIsBinary(info) && ChannelTypeIsBinary(type)) {
                // ok
            } else {
                mcx_log(LOG_ERROR, "Port %s: Set outport reference: Mismatching types", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }
    }

    channel->internalValue = reference;

    return RETURN_OK;
}

static McxStatus ChannelOutSetReferenceFunction(ChannelOut * out, const proc * reference, ChannelType * type) {
    Channel * channel = (Channel *) out;
    ChannelInfo * info = NULL;
    if (!out) {
        mcx_log(LOG_ERROR, "Port: Set outport function: Invalid port");
        return RETURN_ERROR;
    }

    info = &channel->info;
    if (ChannelTypeIsValid(type)) {
        if (!ChannelTypeEq(info->type, type)) {
            mcx_log(LOG_ERROR, "Port %s: Set outport function: Mismatching types", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }
    }

    if (out->data->valueFunction) {
        mcx_log(LOG_ERROR, "Port %s: Set outport function: Reference already set", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    // Save channel procedure
    out->data->valueFunction = (const proc *) reference;

    // Setup value reference to point to internal value
    channel->internalValue = ChannelValueReference(&channel->value);

    return RETURN_OK;
}

static void WarnAboutNaN(LogSeverity level, ChannelInfo * info, TimeInterval * time, size_t * count, size_t * max) {
    if (*max > 0) {
        if (*count < *max) {
            mcx_log(level, "Outport %s at time %f is not a number (NaN)",
                   ChannelInfoGetName(info), time->startTime);
            *count += 1;
            if (*count == *max) {
                mcx_log(level, "This warning will not be shown anymore");
            }
        }
    } else {
        mcx_log(level, "Outport %s at time %f is not a number (NaN)",
               ChannelInfoGetName(info), time->startTime);
    }
}

static McxStatus ChannelOutUpdate(Channel * channel, TimeInterval * time) {
    ChannelOut * out = (ChannelOut *)channel;
    ChannelInfo * info = &channel->info;

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
                MCX_DEBUG_LOG("[%f] CH OUT (%s) (%f, %f)", time->startTime, ChannelInfoGetLogName(info), time->startTime, val);
            }
#endif // MCX_DEBUG
            if (RETURN_OK != ChannelValueSetFromReference(&channel->value, &val)) {
                return RETURN_ERROR;
            }
        } else {
#ifdef MCX_DEBUG
            if (time->startTime < MCX_DEBUG_LOG_TIME) {
                if (ChannelTypeEq(&ChannelTypeDouble, info->type)) {
                    MCX_DEBUG_LOG("[%f] CH OUT (%s) (%f, %f)",
                                  time->startTime,
                                  ChannelInfoGetLogName(info),
                                  time->startTime,
                                  * (double *) channel->internalValue);
                } else {
                    MCX_DEBUG_LOG("[%f] CH OUT (%s)", time->startTime, ChannelInfoGetLogName(info));
                }
            }
#endif // MCX_DEBUG
            if (RETURN_OK != ChannelValueSetFromReference(&channel->value, channel->internalValue)) {
                return RETURN_ERROR;
            }
        }

        // Apply conversion
        if (ChannelTypeEq(info->type, &ChannelTypeDouble) ||
            ChannelTypeEq(info->type, &ChannelTypeInteger)) {
            ChannelValue * val = &channel->value;

            // range
            if (out->data->rangeConversion) {
                if (out->data->rangeConversionIsActive) {
                    Conversion * conversion = (Conversion *) out->data->rangeConversion;
                    retVal = conversion->convert(conversion, val);
                    if (RETURN_OK != retVal) {
                        mcx_log(LOG_ERROR, "Port %s: Update outport: Could not execute range conversion", ChannelInfoGetLogName(info));
                        return RETURN_ERROR;
                    }
                }
            }

            // linear
            if (out->data->linearConversion) {
                Conversion * conversion = (Conversion *) out->data->linearConversion;
                retVal = conversion->convert(conversion, val);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "Port %s: Update outport: Could not execute linear conversion", ChannelInfoGetLogName(info));
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


    if (ChannelTypeEq(&ChannelTypeDouble, info->type)) {
        const double * val = NULL;

        {
            val = &channel->value.value.d;
        }

        if (isnan(*val))
        {
            switch (out->data->nanCheck) {

            case NAN_CHECK_ALWAYS:
                mcx_log(LOG_ERROR, "Outport %s at time %f is not a number (NaN)",
                       ChannelInfoGetName(info), time->startTime);
                return RETURN_ERROR;

            case NAN_CHECK_CONNECTED:
                if (conns->Size(conns) > 0) {
                    mcx_log(LOG_ERROR, "Outport %s at time %f is not a number (NaN)",
                           ChannelInfoGetName(info), time->startTime);
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
    return channel->internalValue;
}

static McxStatus ChannelLocalUpdate(Channel * channel, TimeInterval * time) {
    return RETURN_OK;
}

static int ChannelLocalIsValid(Channel * channel) {
    return (channel->internalValue != NULL);
}

// TODO: Unify with ChannelOutsetReference (similar code)
static McxStatus ChannelLocalSetReference(ChannelLocal * local,
                                          const void * reference,
                                          ChannelType * type) {
    Channel * channel = (Channel *) local;
    ChannelInfo * info = NULL;

    info = &channel->info;
    if (!info) {
        mcx_log(LOG_ERROR, "Port: Set local value reference: Port not set up");
        return RETURN_ERROR;
    }
    if (channel->internalValue
        && !(info->defaultValue && channel->internalValue == ChannelValueReference(info->defaultValue))) {
        mcx_log(LOG_ERROR, "Port %s: Set local value reference: Reference already set", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }
    if (ChannelTypeIsValid(type)) {
        if (!ChannelTypeEq(info->type, type)) {
            mcx_log(LOG_ERROR, "Port %s: Set local value reference: Mismatching types", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }
    }

    channel->internalValue = reference;

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