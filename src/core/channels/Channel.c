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
#include "core/channels/ChannelValue.h"
#include "core/connections/Connection.h"
#include "core/Conversion.h"

#include "core/channels/ChannelInfo.h"
#include "core/channels/ChannelValueReference.h"
#include "core/channels/Channel.h"
#include "core/channels/ChannelValue.h"
#include "core/channels/Channel_impl.h"

#include <stdarg.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus ReportConnStringError(ChannelInfo * chInfo, const char * prefixFmt, ConnectionInfo * connInfo, const char * fmt, ...) {
    // create format string
    const char * portPrefix = "Port %s: ";
    char * connString = NULL;
    char * formatString = NULL;
    char * prefix = NULL;

    if (connInfo) {
        char * connString = ConnectionInfoConnectionString(connInfo);

        prefix = (char *) mcx_calloc(strlen(portPrefix) - 2 /* format specifier %s */ +
                                     strlen(prefixFmt) - (connInfo == NULL ? 0 : 2 /* format specifier %s */) +
                                     (connInfo == NULL ? 0 : connString ? strlen(connString) : strlen("(null)")) +
                                     strlen(ChannelInfoGetLogName(chInfo)) + 1 /* \0 at the end of the string */,
                                     sizeof(char));
        if (!prefix) {
            goto cleanup;
        }

        sprintf(prefix, portPrefix, ChannelInfoGetLogName(chInfo));
        sprintf(prefix + strlen(prefix), prefixFmt, connString);
    } else {
        prefix = (char *) mcx_calloc(strlen(portPrefix) - 2 /* format specifier %s */ + strlen(prefixFmt) +
                                     strlen(ChannelInfoGetLogName(chInfo)) + 1 /* \0 at the end of the string */,
                                     sizeof(char));
        if (!prefix) {
            goto cleanup;
        }

        sprintf(prefix, portPrefix, ChannelInfoGetLogName(chInfo));
    }

    formatString = (char *) mcx_calloc(strlen(fmt) + strlen(prefix) + 1, sizeof(char));
    if (!formatString) {
        goto cleanup;
    }

    strcat(formatString, prefix);
    strcat(formatString, fmt);

    // log error message
    va_list args;
    va_start(args, fmt);

    mcx_vlog(LOG_ERROR, formatString, args);

    va_end(args);

cleanup:
    // free connection string
    if (connString) {
        mcx_free(connString);
    }

    if (prefix) {
        mcx_free(prefix);
    }

    if (formatString) {
        mcx_free(formatString);
    }

    return RETURN_ERROR;
}

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
    channel->ProvidesValue = NULL;

    channel->IsConnected = NULL;
    channel->IsFullyConnected = NULL;

    return channel;
}


// ----------------------------------------------------------------------
// ChannelIn

// object that is stored in target component that stores
// the channel connection

static void DestroyChannelValueReferencePtr(ChannelValueReference ** ptr) {
    DestroyChannelValueReference(*ptr);
}

static McxStatus ChannelInDataInit(ChannelInData * data) {
    data->increment = 2;

    data->connList.connections = (Connection * *) mcx_malloc(sizeof(Connection *));
    if (!data->connList.connections) {
        mcx_log(LOG_ERROR, "ChannelInDataInit: Allocating space for connections failed");
        return RETURN_ERROR;
    }
    data->connList.capacity = 1;
    data->connList.numConnections = 0;

    data->valueReferences = (ChannelValueReference * *) mcx_malloc(sizeof(ChannelValueReference *));
    if (!data->valueReferences) {
        mcx_log(LOG_ERROR, "ChannelInDataInit: Allocating space for value references failed");
        return RETURN_ERROR;
    }

    data->reference  = NULL;
    data->type = ChannelTypeClone(&ChannelTypeUnknown);
    if (!data->type) {
        mcx_log(LOG_ERROR, "ChannelInDataInit: Cloning ChannelType failed");
        return RETURN_ERROR;
    }

    data->typeConversions = (TypeConversion * *) mcx_malloc(sizeof(TypeConversion *));
    if (!data->typeConversions) {
        mcx_log(LOG_ERROR, "ChannelInDataInit: Allocating space for type conversion objects failed");
        return RETURN_ERROR;
    }
    data->unitConversions = (UnitConversion * *) mcx_malloc(sizeof(UnitConversion *));
    if (!data->unitConversions) {
        mcx_log(LOG_ERROR, "ChannelInDataInit: Allocating space for unit conversion objects failed");
        return RETURN_ERROR;
    }
    data->linearConversion = NULL;
    data->rangeConversion = NULL;

    data->isDiscrete = FALSE;

    return RETURN_OK;
}

static void ChannelInDataDestructor(ChannelInData * data) {

    if (data->linearConversion) {
        object_destroy(data->linearConversion);
    }
    if (data->rangeConversion) {
        object_destroy(data->rangeConversion);
    }
    if (data->type) {
        ChannelTypeDestructor(data->type);
    }

    if (data->valueReferences) {
        mcx_free((void *) data->valueReferences);
    }

    if (data->connList.connections) {
        mcx_free((void *) data->connList.connections);
    }
    if (data->typeConversions) {
        for (int j = 0; j < data->connList.numConnections; j++) {
            object_destroy(data->typeConversions[j]);
        }
        mcx_free((void *) data->typeConversions);
    }
    if (data->unitConversions) {
        for (int j = 0; j < data->connList.numConnections; j++) {
            object_destroy(data->unitConversions[j]);
        }
        mcx_free((void *) data->unitConversions);
    }
}

static McxStatus ChannelInSetReference(ChannelIn * in, void * reference, ChannelType * type) {
    Channel * ch = (Channel *) in;
    ChannelInfo * info = &ch->info;

    if (!in) {
        mcx_log(LOG_ERROR, "Port: Set inport reference: Invalid port");
        return RETURN_ERROR;
    }
    if (in->data.reference) {
        mcx_log(LOG_ERROR, "Port %s: Set inport reference: Reference already set", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    if (ChannelTypeIsValid(type)) {
        if (!info) {
            mcx_log(LOG_ERROR, "Port %s: Set inport reference: Port not set up", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }
        if (!ChannelTypeEq(info->type, type)) {
            if (ChannelInfoIsBinary(info) && ChannelTypeIsBinary(type)) {
                // ok
            } else {
                mcx_log(LOG_ERROR, "Port %s: Set inport reference: Mismatching types", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }
    }

    in->data.reference = reference;
    in->data.type = ChannelTypeClone(type);

    return RETURN_OK;
}

static const void * ChannelInGetValueReference(Channel * channel) {
    ChannelIn * in = (ChannelIn *) channel;
    if (!channel->ProvidesValue(channel)) {
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

    return ChannelValueDataPointer(&channel->value);
}

static McxStatus ChannelInUpdate(Channel * channel, TimeInterval * time) {
    ChannelIn * in = (ChannelIn *) channel;
    ChannelInfo * info = &channel->info;

    McxStatus retVal = RETURN_OK;

    /* if no connection is present we have nothing to update*/
    size_t i = 0;
    size_t numConns = in->data.connList.numConnections;
    for (i = 0; i < numConns; i++) {
        Connection * conn = (Connection *) in->data.connList.connections[i];
        ConnectionInfo * connInfo = &conn->info;
        TypeConversion * typeConv = in->data.typeConversions[i];
        UnitConversion * unitConv = in->data.unitConversions[i];
        ChannelValueReference * valueRef = in->data.valueReferences[i];

        /* Update the connection for the current time */
        if (RETURN_OK != conn->UpdateToOutput(conn, time)) {
            return ReportConnStringError(info, "Update inport for connection %s: ", connInfo, "UpdateToOutput failed");
        }

        // TODO: ideally make conn->GetValueReference return ChannelValueReference
        if (RETURN_OK != ChannelValueReferenceSetFromPointer(valueRef, conn->GetValueReference(conn), conn->GetValueDimension(conn), typeConv)) {
            return ReportConnStringError(info, "Update inport for connection %s: ", connInfo, "ChannelValueReferenceSetFromPointer failed");
        }

        // unit conversion
        if (unitConv) {
            retVal = unitConv->ConvertValueReference(unitConv, valueRef);
            if (RETURN_OK != retVal) {
                return ReportConnStringError(info, "Update inport for connection %s: ", connInfo, "Unit conversion failed");
            }
        }
    }


    // Conversions
    if (numConns > 0) {
        ChannelValue * val =  &channel->value;

        // linear
        if (in->data.linearConversion) {
            Conversion * conversion = (Conversion *) in->data.linearConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute linear conversion", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }

        // range
        if (in->data.rangeConversion) {
            Conversion * conversion = (Conversion *) in->data.rangeConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update inport: Could not execute range conversion", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }
    }

    /* no reference from the component was set, skip updating*/
    if (!in->data.reference || !channel->GetValueReference(channel)) {
        return RETURN_OK;
    }

#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME && ChannelTypeEq(info->type, &ChannelTypeDouble)) {
        MCX_DEBUG_LOG("[%f] CH IN  (%s) (%f, %f)", time->startTime, ChannelInfoGetLogName(info), time->startTime, * (double *) channel->GetValueReference(channel));
    }
#endif // MCX_DEBUG

    if (RETURN_OK != ChannelValueDataSetFromReference(in->data.reference, in->data.type, channel->GetValueReference(channel))) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static int ChannelInIsFullyConnected(Channel * channel) {
    ChannelIn * in = (ChannelIn *) channel;
    size_t i = 0;
    size_t connectedElems = 0;
    size_t channelNumElems = channel->info.dimension ? ChannelDimensionNumElements(channel->info.dimension) : 1;

    for (i = 0; i < in->data.connList.numConnections; i++) {
        Connection * conn = (Connection *) in->data.connList.connections[i];
        ConnectionInfo * info = &conn->info;

        connectedElems += info->targetDimension ? ChannelDimensionNumElements(info->targetDimension) : 1;
    }

    if (connectedElems == channelNumElems) {
        return TRUE;
    }

    return FALSE;
}

static int ChannelInProvidesValue(Channel * channel) {
    if (ChannelInIsFullyConnected(channel)) {
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
    in->data.isDiscrete = TRUE;
}

static int ChannelInIsDiscrete(ChannelIn * in) {
    return in->data.isDiscrete;
}

static int ChannelInIsConnected(Channel * channel) {
    if (ChannelTypeIsValid(channel->info.type) && channel->info.connected) {
        return TRUE;
    } else {
        ChannelIn * in = (ChannelIn *) channel;
        if (in->data.connList.numConnections > 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static Vector * ChannelInGetConnectionInfos(ChannelIn * in) {
    Vector * infos = (Vector*) object_create(Vector);
    size_t numConns = in->data.connList.numConnections;
    size_t i = 0;

    if (!infos) {
        return NULL;
    }

    infos->Setup(infos, sizeof(ConnectionInfo*), NULL, NULL, NULL);

    for (i = 0; i < numConns; i++) {
        Connection * conn = in->data.connList.connections[i];
        ConnectionInfo * connInfo = &conn->info;
        if (RETURN_ERROR == infos->PushBack(infos, &connInfo)) {
            object_destroy(infos);
            return NULL;
        }
    }

    return infos;
}

static ConnectionList * ChannelInGetConnections(ChannelIn * in) {
    return &in->data.connList;
}

static McxStatus ChannelInRegisterConnection(ChannelIn * in, Connection * connection, const char * unit, ChannelType * type) {
    ConnectionInfo * connInfo = &connection->info;
    Channel * channel = (Channel *) in;
    ChannelInfo * inInfo = &channel->info;
    ChannelValueReference * valRef = NULL;
    ConnectionList * connList = &in->data.connList;

    McxStatus retVal = RETURN_OK;

    while (connList->capacity <= connList->numConnections) {
        connList->capacity *= in->data.increment;
        connList->connections = mcx_realloc(connList->connections, sizeof(Connection *) * connList->capacity);
        in->data.typeConversions = mcx_realloc(in->data.typeConversions, sizeof(TypeConversion *) * connList->capacity);
        in->data.unitConversions = mcx_realloc(in->data.unitConversions, sizeof(UnitConversion *) * connList->capacity);
        in->data.valueReferences = mcx_realloc(in->data.valueReferences, sizeof(ChannelValueReference *) * connList->capacity);
        if (!connList->connections || !in->data.typeConversions || !in->data.unitConversions || !in->data.valueReferences) {
            mcx_log(LOG_ERROR, "ChannelInRegisterConnection: (Re-)Allocation of connection related data failed");
            return RETURN_ERROR;
        }
    }
    connList->connections[connList->numConnections] = connection;
    connList->numConnections++;

    ChannelDimension * dimension = connInfo->targetDimension;
    if (dimension && !ChannelDimensionEq(dimension, inInfo->dimension)) {
        ChannelDimension * slice = CloneChannelDimension(dimension);

        retVal = ChannelDimensionAlignIndicesWithZero(slice, inInfo->dimension);
        if (retVal == RETURN_ERROR) {
            ReportConnStringError(inInfo, "Register inport connection %s: ", connInfo, "Normalizing array slice dimension failed");
            return RETURN_ERROR;
        }

        valRef = MakeChannelValueReference(&channel->value, slice);
    } else {
        valRef = MakeChannelValueReference(&channel->value, NULL);
    }

    in->data.valueReferences[connList->numConnections - 1] = valRef;

    if (ChannelTypeEq(ChannelTypeBaseType(inInfo->type), &ChannelTypeDouble)) {
        UnitConversion * conversion = (UnitConversion *) object_create(UnitConversion);

        retVal = conversion->Setup(conversion, unit, inInfo->unitString);
        if (RETURN_ERROR == retVal) {
            return ReportConnStringError(inInfo, "Register inport connection %s: ", connInfo, "Could not set up unit conversion");
        }

        if (conversion->IsEmpty(conversion)) {
            object_destroy(conversion);
        }

        in->data.unitConversions[connList->numConnections - 1] = conversion;
    } else {
        in->data.unitConversions[connList->numConnections - 1] = NULL;
    }

    // setup type conversion
    if (!ChannelTypeConformable(inInfo->type, inInfo->dimension, connection->GetValueType(connection), connection->GetValueDimension(connection))) {
        TypeConversion * typeConv = (TypeConversion *) object_create(TypeConversion);
        retVal = typeConv->Setup(typeConv,
                                 connection->GetValueType(connection),
                                 connection->GetValueDimension(connection),
                                 inInfo->type,
                                 connInfo->targetDimension);
        if (RETURN_ERROR == retVal) {
            return ReportConnStringError(inInfo, "Register inport connection %s: ", connInfo, "Could not set up type conversion");
        }

        in->data.typeConversions[connList->numConnections - 1] = typeConv;
    } else {
        in->data.typeConversions[connList->numConnections - 1] = NULL;
    }

    return retVal;
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
    ChannelValueInit(&channel->value, ChannelTypeClone(info->type));

    // default value
    if (info->defaultValue) {
        ChannelValueSet(&channel->value, info->defaultValue);

        // apply range and linear conversions immediately
        retVal = ConvertRange(info->min, info->max, &channel->value, NULL);
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
        }

        retVal = ConvertLinear(info->scale, info->offset, &channel->value, NULL);
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
        }

        channel->SetDefinedDuringInit(channel);
    }

    // unit conversion is setup when a connection is set

    // min/max conversions are only used for double types
    if (ChannelTypeEq(ChannelTypeBaseType(info->type), &ChannelTypeDouble) ||
        ChannelTypeEq(ChannelTypeBaseType(info->type), &ChannelTypeInteger))
    {
        ChannelValue * min = info->min;
        ChannelValue * max = info->max;

        ChannelValue * scale  = info->scale;
        ChannelValue * offset = info->offset;

        in->data.rangeConversion = (RangeConversion *) object_create(RangeConversion);
        retVal = in->data.rangeConversion->Setup(in->data.rangeConversion, min, max);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup inport: Could not setup range conversion", ChannelInfoGetLogName(info));
            object_destroy(in->data.rangeConversion);
            return RETURN_ERROR;
        } else {
            if (in->data.rangeConversion->IsEmpty(in->data.rangeConversion)) {
                object_destroy(in->data.rangeConversion);
            }
        }

        in->data.linearConversion = (LinearConversion *) object_create(LinearConversion);
        retVal = in->data.linearConversion->Setup(in->data.linearConversion, scale, offset);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup inport: Could not setup linear conversion", ChannelInfoGetLogName(info));
            object_destroy(in->data.linearConversion);
            return RETURN_ERROR;
        } else {
            if (in->data.linearConversion->IsEmpty(in->data.linearConversion)) {
                object_destroy(in->data.linearConversion);
            }
        }
    }

    return RETURN_OK;
}

static void ChannelInDestructor(ChannelIn * in) {
    ChannelInDataDestructor(&in->data);
}

static ChannelIn * ChannelInCreate(ChannelIn * in) {
    Channel * channel = (Channel *) in;
    McxStatus retVal = RETURN_OK;

    retVal = ChannelInDataInit(&in->data);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "ChannelInCreate: ChannelInDataInit failed");
        return NULL;
    }

    // virtual functions
    channel->GetValueReference = ChannelInGetValueReference;
    channel->ProvidesValue     = ChannelInProvidesValue;
    channel->Update            = ChannelInUpdate;
    channel->IsConnected       = ChannelInIsConnected;
    channel->IsFullyConnected  = ChannelInIsFullyConnected;

    in->Setup        = ChannelInSetup;
    in->SetReference = ChannelInSetReference;

    in->GetConnectionInfos = ChannelInGetConnectionInfos;

    in->GetConnections = ChannelInGetConnections;
    in->RegisterConnection = ChannelInRegisterConnection;

    in->IsDiscrete = ChannelInIsDiscrete;
    in->SetDiscrete = ChannelInSetDiscrete;

    return in;
}


// ----------------------------------------------------------------------
// ChannelOut

static McxStatus ChannelOutDataInit(ChannelOutData * data) {
    data->valueFunction = NULL;
    ChannelValueInit(&data->valueFunctionRes, ChannelTypeClone(&ChannelTypeUnknown));
    data->rangeConversion = NULL;
    data->linearConversion = NULL;

    data->rangeConversionIsActive = TRUE;

    data->increment = 2;

    data->connList.numConnections = 0;
    data->connList.capacity = 1;
    data->connList.connections = (Connection * *) mcx_malloc(sizeof(Connection *));
    if (!data->connList.connections) {
        mcx_log(LOG_ERROR, "ChannelOutDataInit: Allocation of connections failed");
        return RETURN_ERROR;
    }

    data->nanCheck = NAN_CHECK_ALWAYS;

    data->countNaNCheckWarning = 0;
    data->maxNumNaNCheckWarning = 0;


    return RETURN_OK;
}

static void ChannelOutDataDestructor(ChannelOutData * data) {
    size_t i = 0;

    if (data->rangeConversion) {
        object_destroy(data->rangeConversion);
    }
    if (data->linearConversion) {
        object_destroy(data->linearConversion);
    }

    if (data->connList.connections) {
        for (i = 0; i < data->connList.numConnections; i++) {
            if (data->connList.connections[i]) {
                object_destroy(data->connList.connections[i]);
            }
        }
        mcx_free(data->connList.connections);
    }

    ChannelValueDestructor(&data->valueFunctionRes);
}

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
    ChannelValueInit(&channel->value, ChannelTypeClone(info->type));

    // default value
    if (info->defaultValue) {
        channel->internalValue = ChannelValueDataPointer(channel->info.defaultValue);
    }


    // min/max conversions are only used for double types
    if (ChannelTypeEq(ChannelTypeBaseType(info->type), &ChannelTypeDouble)
        || ChannelTypeEq(ChannelTypeBaseType(info->type), &ChannelTypeInteger))
    {
        out->data.rangeConversion = (RangeConversion *) object_create(RangeConversion);
        retVal = out->data.rangeConversion->Setup(out->data.rangeConversion, min, max);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup outport: Could not setup range conversion", ChannelInfoGetLogName(info));
            object_destroy(out->data.rangeConversion);
            return RETURN_ERROR;
        } else {
            if (out->data.rangeConversion->IsEmpty(out->data.rangeConversion)) {
                object_destroy(out->data.rangeConversion);
            }
        }

        out->data.linearConversion = (LinearConversion *) object_create(LinearConversion);
        retVal = out->data.linearConversion->Setup(out->data.linearConversion, scale, offset);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Port %s: Setup outport: Could not setup linear conversion", ChannelInfoGetLogName(info));
            object_destroy(out->data.linearConversion);
            return RETURN_ERROR;
        } else {
            if (out->data.linearConversion->IsEmpty(out->data.linearConversion)) {
                object_destroy(out->data.linearConversion);
            }
        }
    }

    if (!config) {
        mcx_log(LOG_DEBUG, "Port %s: Setup outport: No config available", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    out->data.nanCheck = config->nanCheck;
    out->data.maxNumNaNCheckWarning = config->nanCheckNumMessages;

    return RETURN_OK;
}

static McxStatus ChannelOutRegisterConnection(ChannelOut * out, Connection * connection) {
    ChannelInfo * outInfo = &((Channel *) out)->info;
    ConnectionInfo * connInfo = &connection->info;
    ConnectionList * connList = &out->data.connList;

    // TODO: do we have to check that channelout and connection match
    // in type/dimension?

    while (connList->capacity <= connList->numConnections) {
        connList->capacity *= out->data.increment;
        connList->connections = mcx_realloc(connList->connections, connList->capacity * sizeof(Connection *));
        if (!connList->connections) {
            return ReportConnStringError(outInfo, "Register outport connection %s: ", connInfo, "Could not set up connections (realloc failed)");
        }
    }
    connList->connections[connList->numConnections] = connection;
    connList->numConnections++;

    return RETURN_OK;
}

static const void * ChannelOutGetValueReference(Channel * channel) {
    ChannelOut * out = (ChannelOut *) channel;
    ChannelInfo * info = &channel->info;

    // check if out is initialized
    if (!channel->ProvidesValue(channel)) {
        mcx_log(LOG_ERROR, "Port %s: Get value reference: No Value Reference", ChannelInfoGetLogName(info));
        return NULL;
    }

    return ChannelValueDataPointer(&channel->value);
}

static const proc * ChannelOutGetFunction(ChannelOut * out) {
    return out->data.valueFunction;
}

static ConnectionList * ChannelOutGetConnections(ChannelOut * out) {
    return &out->data.connList;
}

static int ChannelOutProvidesValue(Channel * channel) {
    return (NULL != channel->internalValue);
}

static int ChannelOutIsConnected(Channel * channel) {
    if (channel->info.connected) {
        return TRUE;
    } else {
        ChannelOut * out = (ChannelOut *) channel;
        if (out->data.connList.numConnections) {
            return TRUE;
        }
    }

    return FALSE;
}

static int ChannelOutIsFullyConnected(Channel * channel) {
    ChannelOut * out = (ChannelOut *) channel;

    if (ChannelTypeIsArray(channel->info.type)) {
        ConnectionList * conns = &out->data.connList;
        size_t i = 0;
        size_t num_elems = ChannelDimensionNumElements(channel->info.dimension);

        int * connected = (int *) mcx_calloc(num_elems, sizeof(int));
        if (!connected) {
            mcx_log(LOG_ERROR, "ChannelOutIsFullyConnected: Not enough memory");
            return -1;
        }

        for (i = 0; i < conns->numConnections; i++) {
            Connection * conn = out->data.connList.connections[i];
            ConnectionInfo * info = &conn->info;
            size_t j = 0;

            for (j = 0; j < ChannelDimensionNumElements(info->sourceDimension); i++) {
                size_t idx = ChannelDimensionGetIndex(info->sourceDimension, j, channel->info.type->ty.a.dims);
                connected[idx] = 1;
            }
        }

        for (i = 0; i < num_elems; i++) {
            if (!connected[i]) {
                return FALSE;
            }
        }

        return TRUE;
    } else {
        return ChannelOutIsConnected(channel);
    }
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
        && !(info->defaultValue && channel->internalValue == ChannelValueDataPointer(info->defaultValue))) {
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

    if (out->data.valueFunction) {
        mcx_log(LOG_ERROR, "Port %s: Set outport function: Reference already set", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    // Save channel procedure
    out->data.valueFunction = (const proc *) reference;

    // Initialize (and allocate necessary memory)
    ChannelValueInit(&out->data.valueFunctionRes, ChannelTypeClone(type));

    // Setup value reference to point to internal value
    channel->internalValue = ChannelValueDataPointer(&channel->value);

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

    ConnectionList * conns = &out->data.connList;

    ChannelValue * val = NULL;

    McxStatus retVal = RETURN_OK;

    time->endTime = time->startTime;
    {
        size_t j = 0;

        // Set Value
        if (out->GetFunction(out)) {
            // function value
            proc * p = (proc *) out->GetFunction(out);
            if (RETURN_ERROR == p->fn(time, p->env, &out->data.valueFunctionRes)) {
                mcx_log(LOG_ERROR, "Port %s: Update outport: Function failed", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
#ifdef MCX_DEBUG
            if (time->startTime < MCX_DEBUG_LOG_TIME) {
                MCX_DEBUG_LOG("[%f] CH OUT (%s) (%f, %f)",
                              time->startTime,
                              ChannelInfoGetLogName(info),
                              time->startTime,
                              out->data.valueFunctionRes.value.d);
            }
#endif // MCX_DEBUG
            if (RETURN_OK != ChannelValueSetFromReference(&channel->value, ChannelValueDataPointer(&out->data.valueFunctionRes))) {
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
                mcx_log(LOG_ERROR, "Port %s: Update outport: Setting value failed", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }

        val = &channel->value;

        // range conversion
        if (out->data.rangeConversion) {
            if (out->data.rangeConversionIsActive) {
                Conversion * conversion = (Conversion *) out->data.rangeConversion;
                retVal = conversion->convert(conversion, val);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "Port %s: Update outport: Could not execute range conversion", ChannelInfoGetLogName(info));
                    return RETURN_ERROR;
                }
            }
        }

        // linear conversion
        if (out->data.linearConversion) {
            Conversion * conversion = (Conversion *) out->data.linearConversion;
            retVal = conversion->convert(conversion, val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Port %s: Update outport: Could not execute linear conversion", ChannelInfoGetLogName(info));
                return RETURN_ERROR;
            }
        }

        // Notify connections of new values
        channel->SetDefinedDuringInit(channel);
        for (j = 0; j < conns->numConnections; j++) {
            Connection * connection = conns->connections[j];
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
            switch (out->data.nanCheck) {

            case NAN_CHECK_ALWAYS:
                mcx_log(LOG_ERROR, "Outport %s at time %f is not a number (NaN)",
                       ChannelInfoGetName(info), time->startTime);
                return RETURN_ERROR;

            case NAN_CHECK_CONNECTED:
                if (conns->numConnections > 0) {
                    mcx_log(LOG_ERROR, "Outport %s at time %f is not a number (NaN)",
                           ChannelInfoGetName(info), time->startTime);
                    return RETURN_ERROR;
                } else {
                    WarnAboutNaN(LOG_WARNING, info, time, &out->data.countNaNCheckWarning, &out->data.maxNumNaNCheckWarning);
                    break;
                }

            case NAN_CHECK_NEVER:
                WarnAboutNaN((conns->numConnections > 0) ? LOG_ERROR : LOG_WARNING,
                             info, time, &out->data.countNaNCheckWarning, &out->data.maxNumNaNCheckWarning);
                break;
            }
        }
    }


    return RETURN_OK;
}

static void ChannelOutDestructor(ChannelOut * out) {
    ChannelOutDataDestructor(&out->data);
}

static ChannelOut * ChannelOutCreate(ChannelOut * out) {
    Channel * channel = (Channel *) out;
    McxStatus retVal = RETURN_OK;

    retVal = ChannelOutDataInit(&out->data);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "ChannelOutCreate: ChannelOutDataInit failed");
        return NULL;
    }

    // virtual functions
    channel->GetValueReference = ChannelOutGetValueReference;
    channel->ProvidesValue     = ChannelOutProvidesValue;
    channel->Update            = ChannelOutUpdate;
    channel->IsConnected       = ChannelOutIsConnected;
    channel->IsFullyConnected  = ChannelOutIsFullyConnected;

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

static int ChannelLocalProvidesValue(Channel * channel) {
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
        && !(info->defaultValue && channel->internalValue == ChannelValueDataPointer(info->defaultValue))) {
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
    channel->ProvidesValue     = ChannelLocalProvidesValue;

    channel->IsConnected       = ChannelLocalProvidesValue;

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