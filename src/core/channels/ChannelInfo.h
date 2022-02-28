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

#include "core/channels/ChannelValue.h"
#include "core/channels/VectorChannelInfo.h"

#include "common/status.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



typedef struct ChannelInfo {
    /* vector must be NULL if this is a scalar. It is the *only* way
     * to distinguish between vectors of size 1 and scalar values.
     */
    VectorChannelInfo * vector;

    struct Channel * channel;

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

    ChannelType type;

    int connected;
    int initialValueIsExact;
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
McxStatus ChannelInfoSetType(ChannelInfo * info, ChannelType type);
McxStatus ChannelInfoSetVector(ChannelInfo * info, VectorChannelInfo * vector);

int ChannelInfoIsBinary(const ChannelInfo * info);

McxStatus ChannelInfoSetup(ChannelInfo * info,
                           const char * name,
                           const char * descr,
                           const char * unit,
                           ChannelType  type,
                           const char * id);

McxStatus ChannelInfoSetFrom(ChannelInfo * info, const ChannelInfo * other);



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CHANNELS_CHANNELINFO_H */