/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_STORAGE_RESULTSTORAGE_H
#define MCX_STORAGE_RESULTSTORAGE_H

#include "CentralParts.h"
#include "core/Component.h"
#include "core/Model.h"
#include "core/channels/Channel.h"
#include "core/SubModel.h"
#include "storage/Backends.h"

#include "reader/task/ResultsInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const char * const ChannelStoreSuffix[];

typedef struct ResultsStorage ResultsStorage;

typedef struct ChannelStorage     ChannelStorage;
typedef struct ComponentStorage   ComponentStorage;
typedef struct StorageBackend     StorageBackend;

typedef McxStatus (* fStorageBackendConfigure)(StorageBackend * backend, ResultsStorage * storage, const char * path, int flushEveryStore, int storeAtRuntime);
typedef McxStatus (* fStorageBackendSetup)(StorageBackend * backend);
typedef McxStatus (* fStorageBackendStore)(StorageBackend * backend, ChannelStoreType chType, size_t comp, size_t row);
typedef McxStatus (* fStorageBackendFinished)(StorageBackend * backend);

extern const struct ObjectClass _StorageBackend;

struct StorageBackend {
    Object _; // super class first

    fStorageBackendConfigure Configure;
    fStorageBackendSetup     Setup;
    fStorageBackendStore     Store;
    fStorageBackendFinished  Finished;

    int id;

    int needsFullStorage;

    int active;

    struct ResultsStorage * storage;
};

typedef McxStatus (* fResultsStorageRead)(ResultsStorage * storage, ResultsInput * resultsInput, const Config * config, int multiThreaded);
typedef McxStatus (* fResultsStorageSetup)(ResultsStorage * storage, double startTime);
typedef McxStatus (* fResultsStorageFinished)(ResultsStorage * storage);

typedef McxStatus (* fResultsStorageAddModelComponents)(ResultsStorage * storage, SubModel * subModel);
typedef McxStatus (* fResultsStorageAddDefaultBackends)(ResultsStorage * storage, int flushEveryStore);
typedef McxStatus (* fResultsStorageAddBackend)(ResultsStorage * storage, BackendType type, int flushEveryStore);
typedef McxStatus (* fResultsStorageRegisterComponent)(ResultsStorage * storage,
                                                       ComponentStorage * compStore);
typedef McxStatus (* fResultsStorageSetupBackends)(ResultsStorage * storage);
typedef McxStatus (* fResultsStorageStoreModel)(ResultsStorage * storage, SubModel * subModel, double time, StoreLevel level);
typedef McxStatus (* fResultsStorageFinishModel)(ResultsStorage * storage, SubModel * subModel);
typedef McxStatus (* fResultsStorageSetStored)(ResultsStorage * storage, ComponentStorage * compStore, ChannelStoreType chType, double time);
typedef McxStatus (* fResultsStorageSetFinished)(ResultsStorage * storage, ComponentStorage * compStore);
typedef McxStatus (* fResultsStorageStoreBackends)(ResultsStorage * storage, ChannelStoreType chType, size_t i, size_t start, size_t end);
typedef McxStatus (* fResultsStorageFinishBackends)(ResultsStorage * storage);
typedef StoreLevel (* fResultsStorageGetStoreLevel)(ResultsStorage * storage);
typedef void (* fResultsStorageSetStoreLevel)(ResultsStorage * storage, StoreLevel level);

typedef McxStatus (* fResultsStorageSetChannelStoreEnabled)(ResultsStorage * storage, ChannelStoreType type, int enabled);

typedef double (* fResultsStorageGetTime)(ResultsStorage * storage);

extern const struct ObjectClass _ResultsStorage;

struct ResultsStorage {
    Object _; // super class first

    fResultsStorageRead Read;
    fResultsStorageSetup Setup;
    fResultsStorageFinished Finished;

    fResultsStorageStoreModel StoreModel;
    fResultsStorageStoreModel StoreModelOut;
    fResultsStorageStoreModel StoreModelLocal;
    fResultsStorageFinishModel FinishModel;

    fResultsStorageRegisterComponent  RegisterComponent;
    fResultsStorageAddModelComponents AddModelComponents;
    fResultsStorageAddDefaultBackends AddDefaultBackends;
    fResultsStorageAddBackend         AddBackend;
    fResultsStorageSetupBackends      SetupBackends;
    fResultsStorageSetStored          SetStored;
    fResultsStorageStoreBackends      StoreBackends;

    fResultsStorageSetFinished     SetFinished;
    fResultsStorageFinishBackends  FinishBackends;

    fResultsStorageGetStoreLevel GetStoreLevel;
    fResultsStorageSetStoreLevel SetStoreLevel;

    fResultsStorageGetTime GetStartTime;

    fResultsStorageSetChannelStoreEnabled SetChannelStoreEnabled;

    // list of components for saving, maybe not the whole model (therefore no pointer to model)
    // loop model components once and register all components to save
    size_t numComponents;
    ComponentStorage ** componentStorage;
    double * componentStoredTime;
    int * isComponentFinished;

    size_t numComponentsFinished;

    StorageBackend * backends[BACKEND_NUM];

    int storeAtRuntime[BACKEND_NUM];

    char *resultPath;
    int needsFullStorage;

    StoreLevel level;

    int active;

    int channelStoreEnabled[CHANNEL_STORE_NUM];

    double startTime;
};

McxStatus ResultsStorageSetStoreFlag(ResultsStorage * storage, int active);

const char * GetTimeUnitString(void);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_STORAGE_RESULTSTORAGE_H */