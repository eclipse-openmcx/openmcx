/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_DATABUS_IMPL_H
#define MCX_CORE_DATABUS_IMPL_H

#include "objects/Vector.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct ChannelInfo;
struct ChannelIn;
struct ChannelOut;

// ----------------------------------------------------------------------
// DatabusInfo

extern const struct ObjectClass _DatabusInfoData;

typedef struct DatabusInfoData {
    Object _; // base class

    Vector * infos;
} DatabusInfoData;


extern const struct ObjectClass _DatabusInfo;

typedef struct DatabusInfo {
    Object _; // base class

    DatabusInfoData * data;
} DatabusInfo;



// ----------------------------------------------------------------------
// Databus

extern const struct ObjectClass _DatabusData;

typedef struct DatabusData {
    Object _; // base class

    struct ChannelIn ** in;   /**< input channels */
    struct ChannelIn ** inConnected;   /**< connected input channels */
    size_t numInConnected;
    struct ChannelOut ** out; /**< output channels */
    struct ChannelLocal ** local; /**< local (non-connectable) channels */
    struct ChannelLocal ** rtfactor; /**< rtfactor (non-connectable) channels */

    struct DatabusInfo * inInfo;  /**< metadata (size, properties) for \a in */
    struct DatabusInfo * outInfo; /**< metadata (size, properties) for \a out */
    struct DatabusInfo * localInfo; /**< metadata (size, properties) for \a local */
    struct DatabusInfo * rtfactorInfo; /**< metadata (size, properties) for \a local */
} DatabusData;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_DATABUS_IMPL_H */