/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_STORAGE_COMPONENTSTORAGE_H
#define MCX_STORAGE_COMPONENTSTORAGE_H

#include "storage/ResultsStorage.h"
#include "reader/model/components/ComponentResultsInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ChannelInfo ChannelInfo;
typedef struct Channel Channel;
typedef struct ComponentStorage ComponentStorage;

typedef struct ChannelStorage ChannelStorage;
typedef struct Component Component;

typedef enum StoreLevel StoreLevel;

typedef McxStatus (* fComponentStorageRegisterChannel)(ComponentStorage * compStore, ChannelStoreType chType, Channel * channel);
typedef size_t (* fComponentStorageGetChannelNum)(ComponentStorage * compStore);
typedef void (* fComponentStorageDisableStorage)(ComponentStorage * compStore);
typedef McxStatus (* fComponentStorageRead)(ComponentStorage * compStore, ComponentResultsInput * input);

typedef McxStatus (* fComponentStorageSetup)(ComponentStorage * compStore, ResultsStorage * storage, Component * comp, double synchronizationStep, double couplingStep);
typedef McxStatus (* fComponentStorageStore)(ComponentStorage * storage, ChannelStoreType chType, double time, StoreLevel level);
typedef McxStatus (* fComponentStorageFinished)(ComponentStorage * storage);

extern const struct ObjectClass _ComponentStorage;

struct ComponentStorage {
    Object _; // super class first

    fComponentStorageStore StoreChannels;
    fComponentStorageFinished Finished;

    fComponentStorageRegisterChannel RegisterChannel;
    fComponentStorageDisableStorage DisableStorage;

    fComponentStorageRead Read;
    fComponentStorageSetup Setup;

    ResultsStorage * storage;

    ChannelStorage * channels[CHANNEL_STORE_NUM];

    int hasOwnStoreLevel;
    StoreLevel storeLevel;

    /* range and increments */
    int startTimeDefined;
    double startTime; /* smallest time stored */

    int endTimeDefined;
    double endTime; /* largest time stored */

    int stepTimeDefined;
    double stepTime; /* minimum time between stores */
    size_t stepCount; /* minimum number of store calls between actual stores */

    double timeOffset;

    const Component * comp; ///< pointer to the component that should be actually stored, to retrieve e.g. comp-name, channel-infos, ...
};

McxStatus ComponentStorageAllocateMemory(ComponentStorage * compStore);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */


#endif /* MCX_STORAGE_COMPONENTSTORAGE_H */