/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_VECTORCHANNELINFO_H
#define MCX_CORE_CHANNELS_VECTORCHANNELINFO_H

#include "CentralParts.h"
#include "objects/Vector.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct VectorChannelInfo VectorChannelInfo;

struct ChannelInfo;

typedef McxStatus (* fVectorChannelInfoSetup)(VectorChannelInfo * info, const char * name, const char * nameInModel, int isScalar, size_t startIndex, size_t endIndex);
typedef McxStatus (* fVectorChannelInfoAddElement)(VectorChannelInfo * info, struct ChannelInfo * channel, size_t index);
typedef size_t (* fVectorChannelGetIndex)(VectorChannelInfo * info);
typedef const char * (* fVectorChannelGetName)(VectorChannelInfo * info);
typedef struct ChannelInfo * (* fVectorChannelGetElement)(VectorChannelInfo * info, size_t index);
typedef int (* fVectorChannelInfoIsScalar)(VectorChannelInfo * info);

extern const struct ObjectClass _VectorChannelInfo;

struct VectorChannelInfo {
    Object _; // base class

    fVectorChannelInfoSetup      Setup;
    fVectorChannelInfoAddElement AddElement;
    fVectorChannelGetIndex       GetStartIndex;
    fVectorChannelGetIndex       GetEndIndex;
    fVectorChannelGetName        GetName;
    fVectorChannelGetName        GetNameInTool;
    fVectorChannelGetElement     GetElement;
    fVectorChannelInfoIsScalar   IsScalar;


    char * name;
    char * nameInTool;

    int isScalar; // This is necessary because vectors are also used as scalars

    size_t startIndex;
    size_t endIndex;

    Vector * channels; // of ChannelInfo, the elements of the vector
};

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CHANNELS_VECTORCHANNELINFO_H */