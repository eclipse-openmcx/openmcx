/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_CONNECTIONINFO_IMPL_H
#define MCX_CORE_CONNECTIONS_CONNECTIONINFO_IMPL_H

/* for interExtrapolation, DecoupleType */
#include "CentralParts.h"

/* for ChannelType */
#include "core/channels/ChannelValue.h"

/* for ChannelDimension */
#include "core/channels/ChannelDimension.h"

/* for Component */
#include "core/Component.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// ConnectionInfo

extern const struct ObjectClass _ConnectionInfoData;

typedef struct ConnectionInfoData {
    Object _;

    struct Component * sourceComponent;
    struct Component * targetComponent;

    int sourceChannel;
    int targetChannel;

    ChannelDimension * sourceDimension;

    // Decouple Info: If this connection is decoupled because of an algebraic loop
    // in the model (this means that the value of the source for the target is
    // behind one timestep)
    int isDecoupled;

    InterExtrapolatingType isInterExtrapolating;

    InterExtrapolationType interExtrapolationType;
    InterExtrapolationParams * interExtrapolationParams;

    DecoupleType decoupleType;
    int          decouplePriority;

    int hasDiscreteTarget;

} ConnectionInfoData;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_CONNECTIONINFO_IMPL_H */