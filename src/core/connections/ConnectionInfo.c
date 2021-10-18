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
#include "core/channels/ChannelInfo.h"

#include "core/Model.h"
#include "core/Databus.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



int ConnectionInfoIsDecoupled(ConnectionInfo * info) {
    return info->isDecoupled_;
}

void ConnectionInfoSetDecoupled(ConnectionInfo * info) {
    info->isDecoupled_ = TRUE;
}

ChannelType * ConnectionInfoGetType(ConnectionInfo * info) {
    //Return the data type of the corresponding outport of the source component
    Component * source = NULL;
    Databus * db = NULL;
    ChannelInfo * outInfo = NULL;

    if (ChannelTypeIsValid(info->connType_)) {
        return info->connType_;
    }

    if (NULL == info) {
        mcx_log(LOG_DEBUG, "ConnectionInfo: GetType: no info available");
        return &ChannelTypeUnknown;
    }
    source = info->sourceComponent;
    if (NULL == source) {
        char * buffer = ConnectionInfoConnectionString(info);
        mcx_log(LOG_DEBUG, "ConnectionInfo '%s': GetType: no source available", buffer);
        mcx_free(buffer);
        return &ChannelTypeUnknown;
    }
    db = source->GetDatabus(source);
    if (NULL == db) {
        char * buffer = ConnectionInfoConnectionString(info);
        mcx_log(LOG_DEBUG, "ConnectionInfo '%s': GetType: no databus available", buffer);
        mcx_free(buffer);
        return &ChannelTypeUnknown;
    }
    outInfo = DatabusInfoGetChannel(DatabusGetOutInfo(db), info->sourceChannel);
    if (!outInfo) {
        char * buffer = ConnectionInfoConnectionString(info);
        mcx_log(LOG_DEBUG, "ConnectionInfo '%s': GetType: no outinfo available", buffer);
        mcx_free(buffer);
        return &ChannelTypeUnknown;
    }

    info->connType_ = outInfo->type;
    return info->connType_;
}

char * ConnectionInfoConnectionString(ConnectionInfo * info) {
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

    src = info->sourceComponent;
    trg = info->targetComponent;

    srcID = info->sourceChannel;
    trgID = info->targetChannel;


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
        + strlen(ChannelInfoGetName(srcInfo))
        + strlen(trg->GetName(trg))
        + strlen(ChannelInfoGetName(trgInfo))
        + 1 /* terminator */;

    buffer = (char *) mcx_malloc(len * sizeof(char));
    if (!buffer) {
        return NULL;
    }

    sprintf(buffer, "(%s, %s) - (%s, %s)",
            src->GetName(src),
            ChannelInfoGetName(srcInfo),
            trg->GetName(trg),
            ChannelInfoGetName(trgInfo));

    return buffer;
}

McxStatus ConnectionInfoInit(ConnectionInfo * info) {
    info->sourceComponent = NULL;
    info->targetComponent = NULL;

    info->sourceChannel = -1;
    info->targetChannel = -1;

    info->isDecoupled_ = FALSE;
    info->isInterExtrapolating = INTERPOLATING;

    info->interExtrapolationType = INTEREXTRAPOLATION_NONE;
    info->interExtrapolationParams.extrapolationInterval = INTERVAL_SYNCHRONIZATION;
    info->interExtrapolationParams.extrapolationOrder = POLY_CONSTANT;
    info->interExtrapolationParams.interpolationInterval = INTERVAL_COUPLING;
    info->interExtrapolationParams.interpolationOrder = POLY_CONSTANT;

    info->decoupleType = DECOUPLE_DEFAULT;
    info->decouplePriority = 0;

    info->hasDiscreteTarget = FALSE;

    info->connType_ = &ChannelTypeUnknown;

    info->sourceDimension = NULL;
    info->targetDimension = NULL;

    return RETURN_OK;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */