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
// ChannelLocal

// object that represents internal channels for storage
typedef struct ChannelLocalData {
    Object _; // base class

} ChannelLocalData;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CHANNELS_CHANNEL_IMPL_H */