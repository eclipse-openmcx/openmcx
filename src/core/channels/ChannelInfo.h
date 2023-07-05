/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CHANNELINFO_H
#define MCX_CORE_CHANNELS_CHANNELINFO_H

#include "channel_value/ChannelValue.h"
#include "CentralParts.h"
#include "channel_value/ChannelDimension.h"
#include "core/channels/ConnectionStatus.h"

#include "common/status.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



typedef struct ChannelInfo {
    struct Channel * channel;

    // Channel is a scalar iff dimension == NULL
    ChannelDimension * dimension;

    char * name;
    char * nameInTool;
    char * description;
    char * unitString;
    char * id;

    ChannelMode mode;
    int writeResult;

    ChannelValue * min;
    ChannelValue * max;
    ChannelValue * scale;
    ChannelValue * offset;

    ChannelValue * defaultValue;
    ChannelValue * initialValue;

    ChannelType * type;

    int connected;
    int initialValueIsExact;

    ConnectionStatus * connectionStatus;
} ChannelInfo;


McxStatus ChannelInfoInit(ChannelInfo * info);
void ChannelInfoDestroy(ChannelInfo * info);


const char * ChannelInfoGetLogName(const ChannelInfo * info);
const char * ChannelInfoGetName(const ChannelInfo * info);

McxStatus ChannelInfoSetName(ChannelInfo * info, const char * name);
McxStatus ChannelInfoSetNameInTool(ChannelInfo * info, const char * name);
McxStatus ChannelInfoSetID(ChannelInfo * info, const char * name);
McxStatus ChannelInfoSetDescription(ChannelInfo * info, const char * name);
McxStatus ChannelInfoSetUnit(ChannelInfo * info, const char * name);
McxStatus ChannelInfoSetType(ChannelInfo * info, ChannelType * type);

int ChannelInfoIsBinary(const ChannelInfo * info);

// steals `dimension`, pass NULL if scalar
McxStatus ChannelInfoSetup(ChannelInfo * info,
                           const char * name,
                           const char * nameInModel,
                           const char * descr,
                           const char * unit,
                           ChannelType * type,
                           const char * id,
                           ChannelDimension * dimension);

McxStatus ChannelInfoSetFrom(ChannelInfo * info, const ChannelInfo * other);



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CHANNELS_CHANNELINFO_H */