/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CHANNEL_VALUE_REFERENCE_H
#define MCX_CORE_CHANNELS_CHANNEL_VALUE_REFERENCE_H

#include "CentralParts.h"
#include "core/channels/ChannelDimension.h"

#include "core/Conversion.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct ArraySlice {
    ChannelDimension * dimension;
    ChannelValue * ref;
} ArraySlice;

typedef enum ChannelValueRefType {
    CHANNEL_VALUE_REF_VALUE,
    CHANNEL_VALUE_REF_SLICE
} ChannelValueRefType;


extern const struct ObjectClass _ChannelValueRef;

typedef struct ChannelValueRef {
    Object _;

    ChannelValueRefType type;
    union {
        ChannelValue * value;
        ArraySlice * slice;
    } ref;
} ChannelValueRef;

McxStatus
ChannelValueRefSetFromReference(ChannelValueRef * ref, const void * reference, ChannelDimension * srcDimension, TypeConversion * typeConv);
ChannelType * ChannelValueRefGetType(ChannelValueRef * ref);

typedef McxStatus (*fChannelValueRefElemMapFunc)(void * element, size_t idx, ChannelType * type, void * ctx);
McxStatus ChannelValueRefElemMap(ChannelValueRef * ref, fChannelValueRefElemMapFunc fn, void * ctx);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_CORE_CHANNELS_CHANNEL_VALUE_REFERENCE_H