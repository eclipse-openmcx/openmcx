/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CHANNEL_IMPL_H
#define MCX_CORE_CHANNELS_CHANNEL_IMPL_H

#include "CentralParts.h"
#include "objects/ObjectContainer.h"
#include "core/channels/ChannelInfo.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



// ----------------------------------------------------------------------
// ChannelIn

// object that is stored in target component that stores the channel connection
typedef struct ChannelInData {
    Object _; // base class

    ObjectContainer * connections;      // connections (non-overlapping) going into the channel
    ObjectContainer * valueReferences;  // references to non-overlapping parts of ChannelData::value, where
                                        // values gotten from connections are going to be stored

    // ----------------------------------------------------------------------
    // Conversions

    struct TypeConversion * typeConversion;
    struct UnitConversion * unitConversion;
    struct LinearConversion * linearConversion;
    struct RangeConversion * rangeConversion;

    // ----------------------------------------------------------------------
    // Storage in Component

    int isDiscrete;

    void * reference;
    ChannelType * type;
} ChannelInData;

// ----------------------------------------------------------------------
// ChannelOut

// object that is provided to consumer of output channel
typedef struct ChannelOutData {
    Object _; // base class

    // Function pointer that provides the value of the channel when called
    const proc * valueFunction;

    // Used to store results of channel-internal valueFunction calls
    ChannelValue valueFunctionRes;

    // ----------------------------------------------------------------------
    // Conversion

    struct RangeConversion * rangeConversion;
    struct LinearConversion * linearConversion;

    int rangeConversionIsActive;

    // ----------------------------------------------------------------------
    // NaN Handling

    NaNCheckLevel nanCheck;

    size_t countNaNCheckWarning;
    size_t maxNumNaNCheckWarning;

    // ----------------------------------------------------------------------
    // Connections to Consumers

    // A list of all input channels that are connected to this output channel
    ObjectList * connections;

} ChannelOutData;

// ----------------------------------------------------------------------
// ChannelLocal

// object that represents internal channels for storage
typedef struct ChannelLocalData {
    Object _; // base class

} ChannelLocalData;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CHANNELS_CHANNEL_IMPL_H */