/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_CONNECTIONINFO_H
#define MCX_CORE_CONNECTIONS_CONNECTIONINFO_H

#include "core/channels/ChannelInfo.h"
#include "objects/ObjectContainer.h"
#include "CentralParts.h"
#include "core/Component_interface.h"

#define DECOUPLE_DEFAULT DECOUPLE_IFNEEDED

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// ConnectionInfo

struct ConnectionInfo;
typedef struct ConnectionInfo ConnectionInfo;

typedef void (*fConnectionInfoSet)(
      ConnectionInfo * info
    , Component * sourceComponent
    , Component * targetComponent
    , int sourceChannel
    , int targetChannel
    , int isDecoupled
    , InterExtrapolatingType isInterExtrapolating
    , InterExtrapolationType interExtrapolationType
    , InterExtrapolationParams * interExtrapolationParams
    , DecoupleType decoupleType
    , int decouplePriority
);

typedef void (* fConnectionInfoGet)(
      ConnectionInfo * info
    , Component ** sourceComponent
    , Component ** targetComponent
    , int * sourceChannel
    , int * targetChannel
    , int * isDecoupled
    , InterExtrapolatingType * isInterExtrapolating
    , InterExtrapolationType * interExtrapolationType
    , InterExtrapolationParams ** interExtrapolationParams
    , DecoupleType * decoupleType
    , int * decouplePriority
);

typedef ConnectionInfo * (* fConnectionInfoClone)(
    ConnectionInfo * info);

typedef ConnectionInfo * (* fConnectionInfoCopy)(
    ConnectionInfo * info,
    int              sourceChannel,
    int              targetChannel);

typedef int (* fConnectionInfoGetChannelID)(ConnectionInfo * info);

typedef Component * (* fConnectionInfoGetSourceComponent)(ConnectionInfo * info);
typedef Component * (* fConnectionInfoGetTargetComponent)(ConnectionInfo * info);

typedef DecoupleType (* fConnectionInfoGetDecoupleType)(ConnectionInfo * info);

typedef ChannelType (* fConnectionInfoGetType)(ConnectionInfo * info);

typedef int         (* fConnectionInfoGetBool)(ConnectionInfo * info);
typedef int         (* fConnectionInfoGetInt)(ConnectionInfo * info);
typedef InterExtrapolatingType (* fConnectionInfoGetInterExtrapolating)(ConnectionInfo * info);

typedef void        (* fConnectionInfoSetVoid)(ConnectionInfo * info);
typedef void        (* fConnectionInfoSetInterExtrapolating)(ConnectionInfo * info, InterExtrapolatingType isInterExtrapolating);
typedef char *      (* fConnectionInfoConnectionString)(ConnectionInfo * info);

typedef InterExtrapolationType (* fConnectionInfoGetInterExtraType)(ConnectionInfo * info);
typedef InterExtrapolationParams * (* fConnectionInfoGetInterExtraParams)(ConnectionInfo * info);

typedef void (* fConnectionInfoSetInterExtraType)(ConnectionInfo * info, InterExtrapolationType type);

typedef int (*fConnectionHasDiscreteTarget)(ConnectionInfo * info);
typedef void (*fConnectionSetDiscreteTarget)(ConnectionInfo * info);

extern const struct ObjectClass _ConnectionInfo;

struct ConnectionInfo {
    Object _; // base class

    fConnectionInfoSet Set;
    fConnectionInfoGet Get;

    fConnectionInfoClone Clone;
    fConnectionInfoCopy Copy;

    fConnectionInfoGetChannelID GetSourceChannelID;
    fConnectionInfoGetChannelID GetTargetChannelID;

    fConnectionInfoGetSourceComponent  GetSourceComponent;
    fConnectionInfoGetTargetComponent  GetTargetComponent;

    fConnectionInfoGetDecoupleType     GetDecoupleType;
    fConnectionInfoGetInt              GetDecouplePriority;

    fConnectionInfoGetBool IsDecoupled;
    fConnectionInfoSetVoid SetDecoupled;

    fConnectionInfoGetInterExtrapolating GetInterExtrapolating;
    fConnectionInfoSetInterExtrapolating SetInterExtrapolating;

    fConnectionHasDiscreteTarget HasDiscreteTarget;
    fConnectionSetDiscreteTarget SetDiscreteTarget;

    fConnectionInfoGetType GetType;

    fConnectionInfoConnectionString ConnectionString;

    fConnectionInfoGetInterExtraType GetInterExtraType;
    fConnectionInfoGetInterExtraParams GetInterExtraParams;

    fConnectionInfoSetInterExtraType SetInterExtraType;

    struct ConnectionInfoData * data;

    ChannelType connType;
} ;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_CONNECTIONINFO_H */