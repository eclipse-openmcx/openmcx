/********************************************************************************
 * Copyright (c) 2022 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_CONNECTIONINFO_H
#define MCX_CORE_CONNECTIONS_CONNECTIONINFO_H

#include "CentralParts.h"
#include "core/Component_interface.h"
#include "core/channels/ChannelDimension.h"

#define DECOUPLE_DEFAULT DECOUPLE_IFNEEDED

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct Component Component;


typedef struct ConnectionInfo {
    Component * sourceComponent;
    Component * targetComponent;

    int sourceChannel;
    int targetChannel;

    // Decouple Info: If this connection is decoupled because of an algebraic loop
    // in the model (this means that the value of the source for the target is
    // behind one timestep)
    int isDecoupled_;

    int hasDiscreteTarget;

    ChannelType * connType_;

    InterExtrapolatingType isInterExtrapolating;

    InterExtrapolationType interExtrapolationType;
    InterExtrapolationParams interExtrapolationParams;

    DecoupleType decoupleType;
    int decouplePriority;

    ChannelDimension * sourceDimension;
    ChannelDimension * targetDimension;
} ConnectionInfo;


McxStatus ConnectionInfoInit(ConnectionInfo * info);


ChannelType * ConnectionInfoGetType(ConnectionInfo * info);

int ConnectionInfoIsDecoupled(ConnectionInfo * info);
void ConnectionInfoSetDecoupled(ConnectionInfo * info);

char * ConnectionInfoConnectionString(ConnectionInfo * info);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_CONNECTIONINFO_H */