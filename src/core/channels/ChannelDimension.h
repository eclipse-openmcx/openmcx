/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CHANNELDIMENSION_H
#define MCX_CORE_CHANNELS_CHANNELDIMENSION_H

#include "CentralParts.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const struct ObjectClass _ChannelDimension;

typedef struct ChannelDimension {
    Object _; // super class first

    size_t num;
    size_t * startIdxs;
    size_t * endIdxs;
} ChannelDimension;


McxStatus ChannelDimensionSetup(ChannelDimension * dimension, size_t num);
McxStatus ChannelDimensionSetDimension(ChannelDimension * dimension, size_t num, size_t start, size_t end);
ChannelDimension * ChannelDimensionClone(ChannelDimension * dimension);
size_t ChannelDimensionNumElements(ChannelDimension * dimension);
ChannelDimension * ChannelDimensionClone(ChannelDimension * dimension);
int ChannelDimensionEq(ChannelDimension * first, ChannelDimension * second);
int ChannelDimensionIncludedIn(const ChannelDimension * first, const ChannelDimension * second);
char * ChannelDimensionString(const ChannelDimension * dimension);
McxStatus ChannelDimensionNormalize(ChannelDimension * target, ChannelDimension * base);
ChannelType * ChannelDimensionToChannelType(ChannelDimension * dimension, ChannelType * sourceType);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_CORE_CHANNELS_CHANNELDIMENSION_H