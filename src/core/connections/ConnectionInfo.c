/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/connections/ConnectionInfo.h"
#include "core/connections/ConnectionInfo_impl.h"

#include "core/Model.h"
#include "core/Databus.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static ConnectionInfoData * ConnectionInfoDataCreate(ConnectionInfoData * data) {
    data->sourceComponent = NULL;
    data->targetComponent = NULL;

    data->sourceChannel = -1;
    data->targetChannel = -1;

    data->isDecoupled = FALSE;
    data->isInterExtrapolating = INTERPOLATING;

    data->interExtrapolationType = INTEREXTRAPOLATION_NONE;
    data->interExtrapolationParams = (InterExtrapolationParams *) mcx_malloc(sizeof(InterExtrapolationParams));
    if (!data->interExtrapolationParams) {
        return NULL;
    }
    data->interExtrapolationParams->extrapolationInterval = INTERVAL_SYNCHRONIZATION;
    data->interExtrapolationParams->extrapolationOrder = POLY_CONSTANT;
    data->interExtrapolationParams->interpolationInterval = INTERVAL_COUPLING;
    data->interExtrapolationParams->interpolationOrder = POLY_CONSTANT;

    data->decoupleType     = DECOUPLE_DEFAULT;
    data->decouplePriority = 0;

    data->hasDiscreteTarget = FALSE;

    return data;
}

static void ConnectionInfoDataDestructor(ConnectionInfoData * data) {
    if (data->interExtrapolationParams) {
        mcx_free(data->interExtrapolationParams);
        data->interExtrapolationParams = NULL;
    }
}

OBJECT_CLASS(ConnectionInfoData, Object);


static void ConnectionInfoSet(
    ConnectionInfo * info,
    Component * sourceComponent,
    Component * targetComponent,
    int sourceChannel,
    int targetChannel,
    int isDecoupled,
    InterExtrapolatingType isInterExtrapolating,
    InterExtrapolationType interExtrapolationType,
    InterExtrapolationParams * interExtrapolationParams,
    DecoupleType decoupleType,
    int decouplePriority
) {

    info->data->sourceComponent = sourceComponent;
    info->data->targetComponent = targetComponent;
    info->data->sourceChannel = sourceChannel;
    info->data->targetChannel = targetChannel;
    info->data->isDecoupled = isDecoupled;
    info->data->isInterExtrapolating = isInterExtrapolating;
    info->data->interExtrapolationType = interExtrapolationType;
    info->data->interExtrapolationParams = interExtrapolationParams;
    info->data->decoupleType = decoupleType;
    info->data->decouplePriority = decouplePriority;
}

static void ConnectionInfoGet(
    ConnectionInfo * info,
    Component ** sourceComponent,
    Component ** targetComponent,
    int * sourceChannel,
    int * targetChannel,
    int * isDecoupled,
    InterExtrapolatingType * isInterExtrapolating,
    InterExtrapolationType * interExtrapolationType,
    InterExtrapolationParams ** interExtrapolationParams,
    DecoupleType * decoupleType,
    int * decouplePriority
) {

    * sourceComponent = info->data->sourceComponent;
    * targetComponent = info->data->targetComponent;
    * sourceChannel = info->data->sourceChannel;
    * targetChannel = info->data->targetChannel;
    * isDecoupled = info->data->isDecoupled;
    * isInterExtrapolating = info->data->isInterExtrapolating;
    * interExtrapolationType = info->data->interExtrapolationType;
    * interExtrapolationParams = info->data->interExtrapolationParams;
    * decoupleType = info->data->decoupleType;
    * decouplePriority = info->data->decouplePriority;
}

static ConnectionInfo * ConnectionInfoClone(ConnectionInfo * info) {
    ConnectionInfo * clone = (ConnectionInfo *) object_create(ConnectionInfo);

    if (!clone) {
        return NULL;
    }

    clone->data->sourceComponent = info->data->sourceComponent;
    clone->data->targetComponent = info->data->targetComponent;

    clone->data->sourceChannel = info->data->sourceChannel;
    clone->data->targetChannel = info->data->targetChannel;

    clone->data->isDecoupled          = info->data->isDecoupled;
    clone->data->isInterExtrapolating = info->data->isInterExtrapolating;

    clone->data->interExtrapolationType = info->data->interExtrapolationType;

    clone->data->interExtrapolationParams->extrapolationInterval  = info->data->interExtrapolationParams->extrapolationInterval;
    clone->data->interExtrapolationParams->extrapolationOrder     = info->data->interExtrapolationParams->extrapolationOrder;
    clone->data->interExtrapolationParams->interpolationInterval  = info->data->interExtrapolationParams->interpolationInterval;
    clone->data->interExtrapolationParams->interpolationOrder     = info->data->interExtrapolationParams->interpolationOrder;

    clone->data->decoupleType     = info->data->decoupleType;
    clone->data->decouplePriority = info->data->decouplePriority;

    clone->data->hasDiscreteTarget = info->data->hasDiscreteTarget;

    return clone;
}

static ConnectionInfo * ConnectionInfoCopy(
    ConnectionInfo * info,
    int              sourceChannel,
    int              targetChannel) {

    ConnectionInfo * copy = info->Clone(info);

    copy->data->sourceChannel = sourceChannel;
    copy->data->targetChannel = targetChannel;

    return copy;
}

static int ConnectionInfoGetDecouplePriority(ConnectionInfo * info) {
    return info->data->decouplePriority;
}

static int ConnectionInfoGetSourceChannelID(ConnectionInfo * info) {
    return info->data->sourceChannel;
}

static int ConnectionInfoGetTargetChannelID(ConnectionInfo * info) {
    return info->data->targetChannel;
}

static Component * ConnectionInfoGetSourceComponent(ConnectionInfo * info) {
    return info->data->sourceComponent;
}

static Component * ConnectionInfoGetTargetComponent(ConnectionInfo * info) {
    return info->data->targetComponent;
}

static DecoupleType ConnectionInfoGetDecoupleType(ConnectionInfo * info) {
    return info->data->decoupleType;
}

static int ConnectionInfoIsDecoupled(ConnectionInfo * info) {
    return info->data->isDecoupled;
}

static void ConnectionInfoSetDecoupled(ConnectionInfo * info) {
    info->data->isDecoupled = TRUE;
}

static InterExtrapolatingType ConnectionInfoGetInterExtrapolating(ConnectionInfo * info) {
    return info->data->isInterExtrapolating;
}

static void ConnectionInfoSetInterExtrapolating(ConnectionInfo * info, InterExtrapolatingType isInterExtrapolating) {
    info->data->isInterExtrapolating = isInterExtrapolating;
}

static ChannelType ConnectionInfoGetType(ConnectionInfo * info) {
    //Return the data type of the corresponding outport of the source component
    Component * source = NULL;
    Databus * db = NULL;
    ChannelInfo * outInfo = NULL;

    if (CHANNEL_UNKNOWN != info->connType) {
        return info->connType;
    }

    if (NULL == info) {
        mcx_log(LOG_DEBUG, "ConnectionInfo: GetType: no info available");
        return CHANNEL_UNKNOWN;
    }
    source = info->GetSourceComponent(info);
    if (NULL == source) {
        char * buffer = info->ConnectionString(info);
        mcx_log(LOG_DEBUG, "ConnectionInfo '%s': GetType: no source available", buffer);
        mcx_free(buffer);
        return CHANNEL_UNKNOWN;
    }
    db = source->GetDatabus(source);
    if (NULL == db) {
        char * buffer = info->ConnectionString(info);
        mcx_log(LOG_DEBUG, "ConnectionInfo '%s': GetType: no databus available", buffer);
        mcx_free(buffer);
        return CHANNEL_UNKNOWN;
    }
    outInfo = DatabusInfoGetChannel(DatabusGetOutInfo(db), info->GetSourceChannelID(info));
    if (!outInfo) {
        char * buffer = info->ConnectionString(info);
        mcx_log(LOG_DEBUG, "ConnectionInfo '%s': GetType: no outinfo available", buffer);
        mcx_free(buffer);
        return CHANNEL_UNKNOWN;
    }

    info->connType = outInfo->GetType(outInfo);
    return info->connType;
}

static char * ConnectionInfoConnectionString(ConnectionInfo * info) {
    Component * src = NULL;
    Component * trg = NULL;

    size_t srcID = 0;
    size_t trgID = 0;

    Databus * srcDB = NULL;
    Databus * trgDB = NULL;

    DatabusInfo * srcDBInfo = NULL;
    DatabusInfo * trgDBInfo = NULL;

    ChannelInfo * srcInfo = NULL;
    ChannelInfo * trgInfo = NULL;

    char * buffer = NULL;

    size_t len = 0;

    if (!info) {
        return NULL;
    }

    src = info->GetSourceComponent(info);
    trg = info->GetTargetComponent(info);

    srcID = info->GetSourceChannelID(info);
    trgID = info->GetTargetChannelID(info);


    if (!src || !trg) {
        return NULL;
    }

    srcDB = src->GetDatabus(src);
    trgDB = src->GetDatabus(trg);

    srcDBInfo = DatabusGetOutInfo(srcDB);
    trgDBInfo = DatabusGetInInfo(trgDB);

    srcInfo = DatabusInfoGetChannel(srcDBInfo, srcID);
    trgInfo = DatabusInfoGetChannel(trgDBInfo, trgID);

    if (!srcInfo || !trgInfo) {
        return NULL;
    }

    len = strlen("(, ) - (, )")
        + strlen(src->GetName(src))
        + strlen(srcInfo->GetName(srcInfo))
        + strlen(trg->GetName(trg))
        + strlen(trgInfo->GetName(trgInfo))
        + 1 /* terminator */;

    buffer = (char *) mcx_malloc(len * sizeof(char));
    if (!buffer) {
        return NULL;
    }

    sprintf(buffer, "(%s, %s) - (%s, %s)",
            src->GetName(src),
            srcInfo->GetName(srcInfo),
            trg->GetName(trg),
            trgInfo->GetName(trgInfo));

    return buffer;
}

static InterExtrapolationType ConnectionInfoGetInterExtraType(ConnectionInfo * info) {
    return info->data->interExtrapolationType;
}

static InterExtrapolationParams * ConnectionInfoGetInterExtraParams(ConnectionInfo * info) {
    return info->data->interExtrapolationParams;
}

static void ConnectionInfoSetInterExtraType(ConnectionInfo * info, InterExtrapolationType type) {
    info->data->interExtrapolationType = type;
}

static void ConnectionInfoDestructor(ConnectionInfo * info) {
    object_destroy(info->data);
}

static int ConnectionHasDiscreteTarget(ConnectionInfo * info) {
    return info->data->hasDiscreteTarget;
}

static void ConnectionSetDiscreteTarget(ConnectionInfo * info) {
    info->data->hasDiscreteTarget = TRUE;
}

static ConnectionInfo * ConnectionInfoCreate(ConnectionInfo * info) {
    info->data = (ConnectionInfoData *) object_create(ConnectionInfoData);

    if (!info->data) {
        return NULL;
    }

    info->Set = ConnectionInfoSet;
    info->Get = ConnectionInfoGet;

    info->Clone = ConnectionInfoClone;
    info->Copy  = ConnectionInfoCopy;

    info->GetSourceChannelID = ConnectionInfoGetSourceChannelID;
    info->GetTargetChannelID = ConnectionInfoGetTargetChannelID;

    info->GetSourceComponent = ConnectionInfoGetSourceComponent;
    info->GetTargetComponent = ConnectionInfoGetTargetComponent;

    info->GetDecoupleType     = ConnectionInfoGetDecoupleType;
    info->GetDecouplePriority = ConnectionInfoGetDecouplePriority;

    info->IsDecoupled = ConnectionInfoIsDecoupled;
    info->SetDecoupled = ConnectionInfoSetDecoupled;

    info->GetInterExtrapolating = ConnectionInfoGetInterExtrapolating;
    info->SetInterExtrapolating = ConnectionInfoSetInterExtrapolating;

    info->HasDiscreteTarget = ConnectionHasDiscreteTarget;
    info->SetDiscreteTarget = ConnectionSetDiscreteTarget;

    info->GetType = ConnectionInfoGetType;

    info->ConnectionString = ConnectionInfoConnectionString;

    info->GetInterExtraType = ConnectionInfoGetInterExtraType;
    info->GetInterExtraParams = ConnectionInfoGetInterExtraParams;

    info->SetInterExtraType = ConnectionInfoSetInterExtraType;

    info->connType = CHANNEL_UNKNOWN;
    return info;
}

OBJECT_CLASS(ConnectionInfo, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */