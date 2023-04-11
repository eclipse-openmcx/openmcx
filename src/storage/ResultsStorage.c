/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"

#include "storage/ResultsStorage.h"
#include "storage/ChannelStorage.h"
#include "storage/ComponentStorage.h"
#include "core/Databus.h"
#include "core/SubModel.h"
#include "util/string.h"

#include "reader/task/BackendInput.h"

#include "storage/StorageBackendCsv.h"
#include "util/compare.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

const char * const ChannelStoreSuffix[] = {"in", "res", "local", "RTFactor"};

// ----------------------------------------------------------------------
// Storage Backend

static void StorageBackendDestructor(void * self) {
}

static StorageBackend * StorageBackendCreate(StorageBackend * backend) {
    backend->Configure = NULL;
    backend->Setup = NULL;
    backend->Store = NULL;
    backend->StoreChannelValues = NULL;
    backend->Finished = NULL;

    backend->id = 0;
    backend->needsFullStorage = 0;
    backend->storage = NULL;

    backend->active = TRUE;

    return backend;
}

OBJECT_CLASS(StorageBackend, Object);

// ----------------------------------------------------------------------
// Storage Backend

static McxStatus StorageStoreBackends(ResultsStorage * storage, ChannelStoreType chType, size_t i, size_t start, size_t end) {
    McxStatus retVal = RETURN_OK;
    McxStatus tmp;
    size_t j = 0;
    size_t k = 0;
    for (j = 0; j < BACKEND_NUM; j++) {
        for (k = start; k <= end; k++) {
            StorageBackend * backend = storage->backends[j];
            if (backend) {
                if (storage->channelStoreEnabled[chType]
                    && backend->active) {
                    tmp = backend->Store(backend, chType, i, k);
                    if (RETURN_ERROR == tmp) {
                        mcx_log(LOG_ERROR, "Results: Store backends: Could not store sucessfully");
                        retVal = RETURN_ERROR;
                    }
                }
            }
        }
    }
    return retVal;
}

static McxStatus StorageFinishBackends(ResultsStorage * storage) {
    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    for (i = 0; i < BACKEND_NUM; i++) {
        StorageBackend * backend = storage->backends[i];
        if (backend) {
            McxStatus localRetVal = backend->Finished(backend);
            if (RETURN_ERROR == localRetVal) {
                mcx_log(LOG_ERROR, "Results: Finish backends: Could not finish sucessfully");
                retVal = RETURN_ERROR;
            }
        }
    }

    return retVal;
}

static McxStatus StorageFinished(ResultsStorage * storage) {
    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    for (i = 0; i < storage->numComponents; i++) {
        McxStatus localRetVal = storage->componentStorage[i]->Finished(storage->componentStorage[i]);
        if (RETURN_ERROR == localRetVal) {
            mcx_log(LOG_ERROR, "Results: Finished: Could not finish element %s", storage->componentStorage[i]->comp->GetName(storage->componentStorage[i]->comp));
            retVal = RETURN_ERROR;
        }
    }

    return retVal;
}

static McxStatus StorageStoreModel(ResultsStorage * storage, SubModel * subModel, double time, StoreLevel level) {
    size_t i = 0;

    // Get values from Components
    for (i = 0; i < storage->numComponents; i++) {
        ComponentStorage * compStore = storage->componentStorage[i];
        if (subModel->IsElement(subModel, (Component *)compStore->comp)) {
            size_t chType = 0;
            for (chType = 0; chType < CHANNEL_STORE_NUM; chType++) {
                compStore->StoreChannels(compStore, (ChannelStoreType) chType, time, level);
            }
        }
    }

    return RETURN_OK;
}

static McxStatus StorageStoreModelOut(ResultsStorage * storage, SubModel * subModel, double time, StoreLevel level) {
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    // Get values from Components
    for (i = 0; i < storage->numComponents; i++) {
        ComponentStorage * compStore = storage->componentStorage[i];
        if (subModel->IsElement(subModel, compStore->comp)) {
            retVal = compStore->StoreChannels(compStore, CHANNEL_STORE_OUT, time, level);
            if (RETURN_OK != retVal) {
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus StorageStoreModelLocal(ResultsStorage * storage, SubModel * subModel, double time, StoreLevel level) {
    size_t i = 0;

    // Get values from Components
    for (i = 0; i < storage->numComponents; i++) {
        ComponentStorage * compStore = storage->componentStorage[i];
        if (subModel->IsElement(subModel, compStore->comp)) {
            compStore->StoreChannels(compStore, CHANNEL_STORE_LOCAL, time, level);
        }
    }

    return RETURN_OK;
}

static McxStatus StorageStoreModelRTFactor(ResultsStorage * storage, SubModel * subModel, double time, StoreLevel level) {
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    // Get values from Components
    for (i = 0; i < storage->numComponents; i++) {
        ComponentStorage * compStore = storage->componentStorage[i];
        if (subModel->IsElement(subModel, compStore->comp)) {
            retVal = compStore->StoreChannels(compStore, CHANNEL_STORE_RTFACTOR, time, level);
            if (RETURN_OK != retVal) {
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus StorageFinishModel(ResultsStorage * storage, SubModel * subModel) {
    size_t i = 0;

    mcx_log(LOG_DEBUG, "Results: Finish model");

    // Get values from Components
    for (i = 0; i < storage->numComponents; i++) {
        ComponentStorage * compStore = storage->componentStorage[i];
        if (subModel->ContainsOrIsElement(subModel, compStore->comp)) {
            compStore->Finished(compStore);
        }
    }

    return RETURN_OK;
}

static McxStatus StorageRegisterComponent(ResultsStorage * storage, ComponentStorage * compStore) {
    storage->numComponents += 1;
    storage->componentStorage
        = (ComponentStorage **) mcx_realloc(storage->componentStorage,
                                           storage->numComponents * sizeof(ComponentStorage *));
    if (!storage->componentStorage) {
        mcx_log(LOG_ERROR, "Results: Register component: Memory allocation for componentStorage failed");
        return RETURN_ERROR;
    }

    storage->componentStorage[storage->numComponents - 1] = compStore;

    // is finished flags
    storage->isComponentFinished = (int *) mcx_realloc(storage->isComponentFinished,
                                                      storage->numComponents * sizeof(int));
    if (!storage->isComponentFinished) {
        mcx_log(LOG_ERROR, "Results: Register component: Memory allocation for is finished flags failed");
        return RETURN_ERROR;
    }
    storage->isComponentFinished[storage->numComponents - 1] = 0;

    return RETURN_OK;
}

static McxStatus StorageAddModelComponents(ResultsStorage * storage, SubModel * subModel) {
    ObjectContainer * comps = subModel->components;
    size_t i = 0;
    McxStatus retVal;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);

        retVal = comp->RegisterStorage(comp, storage);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Results: Could not setup element storage");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

// Does not check if backend already defined
static void StorageRegisterBackend(ResultsStorage * storage, BackendType type, StorageBackend * backend) {
    storage->backends[type] = backend;

    storage->needsFullStorage |= backend->needsFullStorage;
}

static McxStatus StorageAddDefaultBackends(ResultsStorage * storage, int flushEveryStore) {
    return storage->AddBackend(storage, BACKEND_CSV, flushEveryStore);
}

static McxStatus StorageAddBackend(ResultsStorage * storage, BackendType type, int flushEveryStore) {
#if defined(ENABLE_STORAGE)
    McxStatus retVal = RETURN_OK;

    if (storage->backends[type]) {
        mcx_log(LOG_ERROR, "The %s result storage backend can only be added once", GetBackendTypeString(type));
        return RETURN_ERROR;
    }
    if (storage->resultPath) {
        StorageBackend * storeBackend = NULL;
        switch (type) {
        case BACKEND_CSV:
        {
            {
                storeBackend = (StorageBackend *)object_create(StorageBackendCsv);
            }
            break;
        }
        }
        if (NULL == storeBackend) {
            mcx_log(LOG_ERROR, "The %s result storage backend could not be created", GetBackendTypeString(type));
            return RETURN_ERROR;
        }
        retVal = storeBackend->Configure(storeBackend, storage, storage->resultPath, flushEveryStore, (int) storage->storeAtRuntime[type]);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "The %s result storage backend could not be configured", GetBackendTypeString(type));
            object_destroy(storeBackend);
            return RETURN_ERROR;
        }
        StorageRegisterBackend(storage, type, storeBackend);
    }
#endif /* ENABLE_STORAGE */
    return RETURN_OK;
}

static McxStatus StorageSetStoredFull(ResultsStorage * storage, ComponentStorage * compStore, ChannelStoreType chType, double endTime) {
    // nothing to do
    return RETURN_OK;
}

static McxStatus StorageSetStored(ResultsStorage * storage, ComponentStorage * compStore, ChannelStoreType chType, double endTime) {
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    // find index of compStore
    for (i = 0; i < storage->numComponents; i++) {
        if (storage->componentStorage[i] == compStore) {
            break;
        }
    }
    if (i == storage->numComponents) {
        ComponentLog(compStore->comp, LOG_ERROR, "Could not find storage for component");
        return RETURN_ERROR;
    }

    /* we store the rows for the times: [storedTime + 1, endTime] */
    {
        ChannelStorage * chStore = compStore->channels[chType];
        size_t start = 0;
        size_t end = chStore->numValues-1;

        if (0 == chStore->numValues) {
            MCX_DEBUG_LOG("STORE BACKENDS FAILED (%zu) %zu - %zu [%f -> %f)", i, start, end, chStore->lastStored, endTime);
            ComponentLog(compStore->comp, LOG_ERROR, "Invalid storage range");
            return RETURN_ERROR;
        }

#ifdef MCX_DEBUG
        if (chStore->lastStored < MCX_DEBUG_LOG_TIME) {
            MCX_DEBUG_LOG("STORE BACKENDS (%zu) %zu - %zu [%f -> %f)", i, start, end, chStore->lastStored, endTime);
        }
#endif // MCX_DEBUG

        retVal = storage->StoreBackends(storage, chType, i, start, end);
        if (RETURN_OK != retVal) {
            ComponentLog(compStore->comp, LOG_ERROR, "Storing backends failed");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus StorageSetFinishedFull(ResultsStorage * storage, ComponentStorage * compStore) {
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < storage->numComponents; i++) {
        if ((storage->componentStorage[i] == compStore)
            && (!storage->isComponentFinished[i])) {
            size_t chType = 0;

            /* flush stuff */
            MCX_DEBUG_LOG("FINISH STORE (%d)", i);

            for (chType = 0; chType < CHANNEL_STORE_NUM; chType++) {
                if (double_geq(compStore->channels[chType]->lastStored, 0)) { // if used at least once
                    retVal = StorageSetStored(storage, compStore, (ChannelStoreType)chType, compStore->channels[chType]->lastStored);
                    if (RETURN_OK != retVal) {
                        return RETURN_ERROR;
                    }
                }
            }

            /* register as finished */
            storage->isComponentFinished[i] = 1;
            storage->numComponentsFinished += 1;

            break;
        }
    }

    if (storage->numComponentsFinished == storage->numComponents) {
        // store backends
        return storage->FinishBackends(storage);
        // do not need to clear flags, we are done
    }

    return RETURN_OK;
}

static McxStatus StorageSetFinished(ResultsStorage * storage, ComponentStorage * compStore) {
    size_t compIdx = 0;

    for (compIdx = 0; compIdx < storage->numComponents; compIdx++) {
        if ((storage->componentStorage[compIdx] == compStore)
            && (!storage->isComponentFinished[compIdx])) {
            MCX_DEBUG_LOG("FINISH STORE (%d)", compIdx);

            /* register as finished */
            storage->isComponentFinished[compIdx] = 1;
            storage->numComponentsFinished += 1;

            break;
        }
    }

    if (storage->numComponentsFinished == storage->numComponents) {
        // store backends
        return storage->FinishBackends(storage);
        // do not need to clear flags, we are done
    }

    return RETURN_OK;
}

static StoreLevel ResultsStorageGetStoreLevel(ResultsStorage * storage) {
    return storage->level;
}

static void ResultsStorageSetStoreLevel(ResultsStorage * storage, StoreLevel level) {
    storage->level = level;
}

static double ResultsStorageGetStartTime(ResultsStorage * storage) {
    return storage->startTime;
}

static McxStatus ResultsStorageSetChannelStoreEnabled(ResultsStorage * storage, ChannelStoreType type, int enabled) {
    if (type >= CHANNEL_STORE_NUM) {
        mcx_log(LOG_ERROR, "Results: Set channel store enabled: Unknown ChannelStoreType %d", type);
        return RETURN_ERROR;
    }

    storage->channelStoreEnabled[type] = enabled;

    return RETURN_OK;
}

static McxStatus StorageRead(ResultsStorage * storage, ResultsInput * resultsInput, const Config * config, int multiThreaded) {
    BackendsInput * backendsInput = resultsInput->backends;
    size_t i = 0;
    McxStatus retVal = RETURN_OK;

    // read result directory
    if (NULL == storage->resultPath) {
        // the result directory is NOT already given by command-line argument
        if (resultsInput->outputDirectory && strcmp(resultsInput->outputDirectory, "")) {
            storage->resultPath = mcx_string_copy(resultsInput->outputDirectory);
        }
        mcx_log(LOG_DEBUG, "Result directory (from input file): %s", storage->resultPath);
    }
    mcx_log(LOG_INFO, "Result directory: %s", storage->resultPath);

    // read storage level
    storage->level = resultsInput->resultLevel.defined ? resultsInput->resultLevel.value : STORE_SYNCHRONIZATION;

    // read backends
    for (i = 0; i < backendsInput->backends->Size(backendsInput->backends); i++) {
        BackendInput * backendInput = (BackendInput *) backendsInput->backends->At(backendsInput->backends, i);

        if (backendInput->type == BACKEND_CSV) {
            retVal = storage->AddBackend(storage, BACKEND_CSV, config->flushEveryStore);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Could not add %s storage backend", GetBackendTypeString(BACKEND_CSV));
                return RETURN_ERROR;
            }

            if (backendInput->storeAtRuntime.defined) {
                storage->storeAtRuntime[BACKEND_CSV] = backendInput->storeAtRuntime.value;
            }
        }
    }
    return RETURN_OK;
}

static McxStatus StorageSetup(ResultsStorage * storage, double startTime) {
    if (double_lt(startTime, 0.0)) {
        return RETURN_ERROR;
    } else {
        storage->startTime = startTime;
    }

    return RETURN_OK;
}

static McxStatus StorageSetupBackends(ResultsStorage * storage) {
    size_t i = 0;
    int numBackends = 0;

    storage->needsFullStorage = 0;
    for (i = 0; i < BACKEND_NUM; i++) {
        StorageBackend * backend = storage->backends[i];
        McxStatus retVal = RETURN_OK;
        if (backend) {
            numBackends++;
            retVal = backend->Setup(backend);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }
            if (storage->backends[i]->needsFullStorage) {
                storage->needsFullStorage = 1;
            }
        }
    }

    if (storage->needsFullStorage) {
        storage->SetStored = StorageSetStoredFull;
        storage->SetFinished = StorageSetFinishedFull;
    }

    // if no backends are registered, then skip storing results
    if (0 == numBackends) {
        size_t compIdx = 0;
        for (compIdx = 0; compIdx < storage->numComponents; compIdx++) {
            ComponentStorage * comp = storage->componentStorage[compIdx];
            comp->DisableStorage(comp);
        }
    }

    return RETURN_OK;
}

McxStatus ResultsStorageSetStoreFlag(ResultsStorage * storage, int active) {
    storage->active = active;
    return RETURN_OK;
}

static void ResultsStorageDestructor(ResultsStorage * storage) {
    size_t i = 0;

    for (i = 0; i < BACKEND_NUM; i++) {
        if (storage->backends[i]) {
            object_destroy(storage->backends[i]);
        }
    }

    if (storage->componentStorage) {
        /* individual component storages are destroyed in components */
        mcx_free(storage->componentStorage);
    }

    if (storage->isComponentFinished) {
        mcx_free(storage->isComponentFinished);
    }

    if (storage->componentStoredTime) {
        mcx_free(storage->componentStoredTime);
    }

    if (storage->resultPath) {
        mcx_free(storage->resultPath);
    }
}

static ResultsStorage * ResultsStorageCreate(ResultsStorage * storage) {
    size_t i = 0;

    storage->Read       = StorageRead;
    storage->Setup      = StorageSetup;
    storage->StoreModel = StorageStoreModel;
    storage->StoreModelOut = StorageStoreModelOut;
    storage->StoreModelLocal = StorageStoreModelLocal;
    storage->StoreModelRTFactor = StorageStoreModelRTFactor;
    storage->FinishModel = StorageFinishModel;
    storage->Finished   = StorageFinished;

    storage->RegisterComponent  = StorageRegisterComponent;
    storage->AddModelComponents = StorageAddModelComponents;
    storage->AddBackend         = StorageAddBackend;
    storage->AddDefaultBackends = StorageAddDefaultBackends;
    storage->SetupBackends      = StorageSetupBackends;

    storage->SetStored = StorageSetStored;
    storage->StoreBackends = StorageStoreBackends;

    storage->SetFinished = StorageSetFinished;
    storage->FinishBackends = StorageFinishBackends;

    storage->GetStoreLevel = ResultsStorageGetStoreLevel;
    storage->SetStoreLevel = ResultsStorageSetStoreLevel;

    storage->GetStartTime = ResultsStorageGetStartTime;

    storage->SetChannelStoreEnabled = ResultsStorageSetChannelStoreEnabled;

    storage->componentStorage = NULL;
    storage->componentStoredTime = NULL;
    storage->numComponents    = 0;

    for (i = 0; i < BACKEND_NUM; i++) {
        storage->backends[i] = NULL;
        storage->storeAtRuntime[i] = TRUE;
    }

    storage->resultPath = NULL;
    storage->needsFullStorage = 0;

    storage->numComponentsFinished = 0;
    storage->isComponentFinished = NULL;

    storage->level = STORE_NONE;

    storage->startTime = -1.0;

    storage->active = TRUE;

    /* enable all stores by default */
    for (i = 0; i < CHANNEL_STORE_NUM; i++) {
        storage->channelStoreEnabled[i] = 1;
    }

    return storage;
}

const char * GetTimeUnitString(void) {
    static const char unitString[] = DEFAULT_TIME_UNIT;
    return unitString;
}

OBJECT_CLASS(ResultsStorage, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */