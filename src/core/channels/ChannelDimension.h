/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CHANNEL_DIMENSION_H
#define MCX_CORE_CHANNELS_CHANNEL_DIMENSION_H

#include "CentralParts.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



typedef struct ChannelDimension {
    size_t num;

    size_t * startIdxs;
    size_t * endIdxs;
} ChannelDimension;


ChannelDimension * MakeChannelDimension();
ChannelDimension * CloneChannelDimension(const ChannelDimension * dimension);
void DestroyChannelDimension(ChannelDimension ** dimension);

McxStatus ChannelDimensionSetup(ChannelDimension * dimension, size_t num);
McxStatus ChannelDimensionSetDimension(ChannelDimension * dimension, size_t num, size_t start, size_t end);

int ChannelDimensionEq(const ChannelDimension * first, const ChannelDimension * second);
int ChannelDimensionConformsToDimension(const ChannelDimension * first, const ChannelDimension * second);
int ChannelDimensionConformsTo(const ChannelDimension * dimension, const size_t * dims, size_t numDims);
int ChannelDimensionIncludedIn(const ChannelDimension * first, const ChannelDimension * second);

size_t ChannelDimensionNumElements(const ChannelDimension* dimension);
char * ChannelDimensionString(const ChannelDimension * dimension);
size_t ChannelDimensionGetIndex(const ChannelDimension * dimension, size_t elem_idx, const size_t * sizes);
size_t ChannelDimensionGetSliceIndex(const ChannelDimension * dimension, size_t slice_idx, const size_t * dims);

McxStatus ChannelDimensionAlignIndicesWithZero(ChannelDimension * target, const ChannelDimension * base);



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_CORE_CHANNELS_CHANNEL_DIMENSION_H