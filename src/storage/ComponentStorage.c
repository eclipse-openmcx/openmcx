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
#include "core/channels/ChannelInfo.h"
#include "storage/ResultsStorage.h"
#include "storage/ChannelStorage.h"
#include "storage/ComponentStorage.h"

#include "util/compare.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Component Storage

static McxStatus ComponentStorageRead(ComponentStorage * compStore, ComponentResultsInput * input) {
    compStore->storeLevel = input->resultLevel.defined ? input->resultLevel.value : STORE_NONE;
    compStore->hasOwnStoreLevel = input->resultLevel.defined;

    compStore->startTime = input->startTime.defined ? input->startTime.value : -1.0;
    compStore->startTimeDefined = input->startTime.defined;

    compStore->endTime = input->endTime.defined ? input->endTime.value : -1.0;
    compStore->endTimeDefined = input->endTime.defined;

    compStore->stepTime = input->stepTime.defined ? input->stepTime.value : 0.0;
    compStore->stepTimeDefined = input->stepTime.defined;

    compStore->stepCount = input->stepCount.defined ? input->stepCount.value : 0;

    if (!double_eq(compStore->stepTime, 0.0) && compStore->stepCount != 0) {
        mcx_log(LOG_ERROR, "Results: Read element storage: Invalid storage settings: Both step time and step count defined");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ComponentStorageRegisterChannel(ComponentStorage * compStore, ChannelStoreType chType, Channel * channel) {
    ChannelStorage * channels = compStore->channels[chType];

    if (compStore->storeLevel > STORE_NONE) {
        ChannelInfo * info = &channel->info;
        if (compStore->storage->channelStoreEnabled[chType] && info->writeResult) {
            return channels->RegisterChannel(channels, channel);
        }
    }

    return RETURN_OK;
}

McxStatus ComponentStorageAllocateMemory(ComponentStorage * compStore) {
    size_t i = 0;
    for (i = 0; i < CHANNEL_STORE_NUM; i++) {
        McxStatus retVal = ChannelStorageAllocateMemory(compStore->channels[i]);
        if (RETURN_ERROR == retVal) {
            ComponentLog(compStore->comp, LOG_ERROR, "Results: Setup element storage: Could not allocate storage");
            return RETURN_ERROR;
        }
    }
    return RETURN_OK;
}

static McxStatus ComponentStorageStoreChannels(ComponentStorage * compStore, ChannelStoreType chType, double time, StoreLevel level) {
    ChannelStorage *channels = NULL;
    McxStatus retVal;

    if (compStore->storage) {
        if (!compStore->storage->active) {
            return RETURN_OK;
        }
    } else {
        // no storage configured
        MCX_DEBUG_LOG("No storage configured");
        return RETURN_OK;
    }

    channels = compStore->channels[chType];

    time = time + compStore->timeOffset;

    if (level > compStore->storeLevel) {
        return RETURN_OK;
    }
    if (0 == channels->GetChannelNum(channels)) {
        return RETURN_OK;
    }

    /* ignore values before start time and after end time */
    if (compStore->startTimeDefined) {
        if (double_lt(time, compStore->startTime)) {
            return RETURN_OK;
        }
    }
    if (compStore->endTimeDefined) {
        if (double_lt(compStore->endTime, time)) {
            return RETURN_OK;
        }
    }

    /* check if at least stepTime between stores (and this is not the first store)*/
    if (compStore->stepTimeDefined) {
        if (double_geq(channels->lastStored, compStore->startTime)
            && double_gt(channels->lastStored + compStore->stepTime, time)) {
                return RETURN_OK;
        }
    }

    if ((compStore->stepCount > 0)) {
        channels->storeCallNum += 1;
        /* store only after stepCount calls */
        if (((channels->storeCallNum - 1) % compStore->stepCount) != 0) {
            return RETURN_OK;
        }
    }

    if (double_lt(time, channels->lastStored)) {
        ComponentLog(compStore->comp, LOG_ERROR,
            "Results: Setting result for previous time (%.17g s < %.17g s), ignoring value", time, channels->lastStored);
        return RETURN_ERROR;
    }

    retVal = channels->Store(channels, time);
    if (RETURN_ERROR == retVal) {
        ComponentLog(compStore->comp, LOG_ERROR, "Results: Could not store ports for time %.17g s", time);
        return RETURN_ERROR;
    }

    retVal = compStore->storage->SetStored(compStore->storage, compStore, chType, time);
    if (RETURN_OK != retVal) {
        ComponentLog(compStore->comp, LOG_ERROR, "Results: Could not store data for time %.17g s", time);
        return RETURN_ERROR;
    }
    channels->lastStored = time;

    return RETURN_OK;
}

static void ComponentStorageDisableStorage(ComponentStorage * compStore) {
    compStore->storeLevel = STORE_NONE;
}

static McxStatus ComponentStorageSetup(ComponentStorage * compStore, ResultsStorage * storage, Component * comp, double synchronizationStep, double couplingStep) {
    McxStatus retVal = RETURN_OK;

    compStore->comp = comp;
    compStore->storage = storage;

    if (!compStore->hasOwnStoreLevel) {
        compStore->storeLevel = storage->GetStoreLevel(storage);
    }

    if (compStore->comp == NULL || compStore->storage == NULL) {
        mcx_log(LOG_ERROR, "Results: Setup element storage: Invalid arguments");
        return RETURN_ERROR;
    }

    /* if start is uninitialized, set it from the resultsstorage */
    if (compStore->startTime == -1.0) {
        compStore->startTime = storage->GetStartTime(storage);
    }

    /*
     * Handle stepCount for coupling steps > synchronization steps: As soon as
     * coupling steps are larger than synchronization steps, using stepCount for
     * synchronization steps results in unexpected behaviour. In that case the
     * larger coupling steps take the role of the smaller synchronization steps
     * (only coupling steps can be written), and the number of steps written
     * gets lowered by the factor coupling / synchronization.
     */
    if (synchronizationStep * couplingStep != 0.0) {
        compStore->stepCount = (size_t) (compStore->stepCount / ceil(couplingStep / synchronizationStep));
    }

    if (compStore->storeLevel > STORE_NONE) {
        size_t i = 0;

        size_t numberOfPorts[CHANNEL_STORE_NUM] = { 0 };  //avoid empty result files
        numberOfPorts[CHANNEL_STORE_IN] = comp->GetNumWriteInChannels(comp);
        numberOfPorts[CHANNEL_STORE_OUT] = comp->GetNumWriteOutChannels(comp);
        numberOfPorts[CHANNEL_STORE_LOCAL] = comp->GetNumWriteLocalChannels(comp);
        numberOfPorts[CHANNEL_STORE_RTFACTOR] = comp->GetNumWriteRTFactorChannels(comp);

        for (i = 0; i < CHANNEL_STORE_NUM; i++) {
            if (compStore->storage->channelStoreEnabled[i] && (numberOfPorts[i] > 0)) {
                retVal = compStore->channels[i]->Setup(compStore->channels[i], storage->needsFullStorage);
                if (RETURN_OK != retVal) {
                    ComponentLog(compStore->comp, LOG_ERROR, "Results: Setup element storage: Could not setup ports");
                    return RETURN_ERROR;
                }
            }
        }

        retVal = storage->RegisterComponent(storage, compStore);
        if (RETURN_OK != retVal) {
            ComponentLog(compStore->comp, LOG_ERROR, "Results: Setup element storage: Could not register element");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus ComponentStorageFinished(ComponentStorage * compStore) {
    /* flush not yet written but complete rows */
    return compStore->storage->SetFinished(compStore->storage, compStore);
}

static void ComponentStorageDestructor(ComponentStorage * compStore) {
    size_t i = 0;

    for (i = 0; i < CHANNEL_STORE_NUM; i++) {
        object_destroy(compStore->channels[i]);
    }

}

static ComponentStorage * ComponentStorageCreate(ComponentStorage * compStore) {
    size_t i = 0;

    compStore->Read      = ComponentStorageRead;
    compStore->Setup     = ComponentStorageSetup;
    compStore->Finished  = ComponentStorageFinished;

    compStore->RegisterChannel = ComponentStorageRegisterChannel;
    compStore->StoreChannels = ComponentStorageStoreChannels;

    compStore->DisableStorage = ComponentStorageDisableStorage;

    for (i = 0; i < CHANNEL_STORE_NUM; i++) {
        compStore->channels[i] = (ChannelStorage *) object_create(ChannelStorage);
        if (!compStore->channels[i]) {
            return NULL;
        }
    }

    compStore->hasOwnStoreLevel = 0;
    compStore->storeLevel = STORE_NONE;

    compStore->startTimeDefined = FALSE;
    compStore->startTime = -1.0;
    compStore->endTimeDefined = FALSE;
    compStore->endTime = -1.0;

    compStore->stepTimeDefined = FALSE;
    compStore->stepTime = 0.0;
    compStore->stepCount = 0;

    compStore->timeOffset = 0.0;

    compStore->comp = NULL;
    compStore->storage = NULL;

    return compStore;
}

OBJECT_CLASS(ComponentStorage, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */