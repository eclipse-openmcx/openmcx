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

#include "core/channels/ChannelInfo.h"
#include "storage/ChannelStorage.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Channel Storage

static McxStatus ChannelStorageStoreFull(ChannelStorage * channelStore, double time);
static McxStatus ChannelStorageStoreNonFull(ChannelStorage * channelStore, double time);

static size_t ChannelStorageGetChannelNum(ChannelStorage * channelStore) {
    ObjectContainer * channels = channelStore->channels;
    return channels->Size(channels);
}

static McxStatus ChannelStorageRegisterChannelInternal(ChannelStorage * channelStore, Channel * channel) {
    ObjectContainer * channels = channelStore->channels;
    McxStatus retVal;
    ChannelInfo *info = NULL;

    /* if values have already been written, do not allow registering additional channels */
    if (channelStore->values) {
        info = &channel->info;
        mcx_log(LOG_ERROR, "Results: Register port %s: Cannot register ports to storage after values have been stored", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    /* add channel */
    retVal = channels->PushBack(channels, (Object *)object_strong_reference(channel));
    if (RETURN_OK != retVal) {
        info = &channel->info;
        mcx_log(LOG_DEBUG, "Results: Register port %s: Pushback of port failed", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ChannelStorageRegisterChannel(ChannelStorage * channelStore, Channel * channel) {
    ObjectContainer * channels = channelStore->channels;

    if (0 == channels->Size(channels)) {
        ChannelInfo *info = &channel->info;
        mcx_log(LOG_ERROR, "Results: Register port %s: Port storage not yet setup", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    return ChannelStorageRegisterChannelInternal(channelStore, channel);
}

static McxStatus ChannelStorageSetup(ChannelStorage * channelStore, int fullStorage) {
    ChannelInfo timeInfo = { 0 };
    Channel * timeChannel = NULL;
    McxStatus retVal = RETURN_OK;

    channelStore->fullStorage = fullStorage;
    if (channelStore->fullStorage) {
        channelStore->Store = ChannelStorageStoreFull;
    } else {
        channelStore->Store = ChannelStorageStoreNonFull;
    }

    /* add time channel */
    retVal = ChannelInfoInit(&timeInfo);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Results: Setup port storage: Initialization of time port data failed");
        goto cleanup;
    }

    retVal = ChannelInfoSetup(&timeInfo, "Time", "", GetTimeUnitString(), CHANNEL_DOUBLE, "");
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Results: Setup port storage: Could not set up time port data");
        goto cleanup;
    }

    timeChannel = (Channel *) object_create(Channel);
    if (!timeChannel) { /* this can only fail because of no memory */
        mcx_log(LOG_DEBUG, "Results: Setup port storage: No memory for time port");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    retVal = timeChannel->Setup(timeChannel, &timeInfo);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Results: Setup port storage: Could not setup time port");
        goto cleanup;
    }

    retVal = ChannelStorageRegisterChannelInternal(channelStore, timeChannel);

cleanup:
    ChannelInfoDestroy(&timeInfo);
    object_destroy(timeChannel);

    return retVal;
}

static McxStatus ChannelStoragePrint(ChannelStorage * channelStore) {
    /* for debugging purposes only */
    ObjectContainer * channels = channelStore->channels;

    char buffer[256];

    size_t row = 0;
    size_t col = 0;

    size_t colNum = channels->Size(channels);

    for (row = 0; row < channelStore->numValues; row++) {
        int pos = 0;
        pos = sprintf(buffer + pos, "|");
        for (col = 0; col < colNum; col++) {
            ChannelValue val = channelStore->GetValueAt(channelStore, row, col);
            pos = sprintf(buffer + pos, "%s|", ChannelValueToString(&val)) + 1;
        }
        MCX_DEBUG_LOG("%s", buffer);
    }

    return RETURN_OK;
}

static McxStatus ChannelStorageSetValueFromReferenceAt(ChannelStorage * channelStore, size_t row, size_t col, const void * reference) {
    ObjectContainer * channels = channelStore->channels;
    Channel * channel = (Channel *) channels->At(channels, col);

    size_t colNum = channels->Size(channels);

    if (!channel) {
        mcx_log(LOG_ERROR, "Results: Set port store value: Invalid column %d", col);
        return RETURN_ERROR;
    }

    if (row >= channelStore->numValuesAllocated) {
        mcx_log(LOG_ERROR, "Results: Set port store value: Invalid row %d", row);
        return RETURN_ERROR;
    }

    if (row >= channelStore->numValues) {
        ChannelInfo * info = &channel->info;
        ChannelValueInit(&channelStore->values[row * colNum + col], info->type);
    }

    ChannelValueSetFromReference(&channelStore->values[row * colNum + col], reference);

    return RETURN_OK;
}

static McxStatus ChannelStorageStoreFull(ChannelStorage * channelStore, double time) {
    ObjectContainer * channels = channelStore->channels;
    size_t i = 0;
    McxStatus retVal;

    /* first store: allocate memory */
    if (channelStore->numValuesAllocated == 0) {
        size_t newSize = 0;
        channelStore->numValuesAllocated = 1;
        newSize = channelStore->numValuesAllocated * channels->Size(channels) * sizeof(ChannelValue);

        channelStore->values = (ChannelValue *) mcx_realloc(channelStore->values, newSize);
        if (!channelStore->values) {
            mcx_log(LOG_DEBUG, "Results: Store ports: No memory for port values");
            return RETURN_ERROR;
        }
    }

    if (channelStore->numValues + 1 > channelStore->numValuesAllocated) {
        size_t newSize = 0;
        channelStore->numValuesAllocated *= 2;
        newSize = channelStore->numValuesAllocated * channels->Size(channels) * sizeof(ChannelValue);

        channelStore->values = (ChannelValue *) mcx_realloc(channelStore->values, newSize);
        if (!channelStore->values) {
            mcx_log(LOG_DEBUG, "Results: Store ports: No memory for port values");
            return RETURN_ERROR;
        }
    }

    /* set time */
    retVal = ChannelStorageSetValueFromReferenceAt(channelStore, channelStore->numValues, 0, &time);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Results: Store ports: Could not set time %f", time);
        return RETURN_ERROR;
    }

    /* set channels */
    for (i = 1; i < channels->Size(channels); i++) {
        Channel * channel = (Channel *) channels->At(channels, i);
        retVal = ChannelStorageSetValueFromReferenceAt(channelStore,
            channelStore->numValues, i, channel->GetValueReference(channel));
        if (RETURN_OK != retVal) { /* error msg in ChannelStorageSetValueFromReferenceAt */
            ChannelInfo *info = &channel->info;
            mcx_log(LOG_DEBUG, "Results: Error in store port %s", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }
    }
    //    ChannelStoragePrint(channelStore);
    channelStore->numValues += 1;

    return RETURN_OK;
}

static McxStatus ChannelStorageStoreNonFull(ChannelStorage * channelStore, double time) {
    ObjectContainer * channels = channelStore->channels;
    size_t i = 0;
    McxStatus retVal;

    /* first store: allocate memory */
    if (channelStore->numValuesAllocated == 0) {
        size_t newSize = 0;
        channelStore->numValuesAllocated = 1;
        newSize = channelStore->numValuesAllocated * channels->Size(channels) * sizeof(ChannelValue);
        channelStore->values = (ChannelValue *) mcx_realloc(channelStore->values, newSize);
        if (!channelStore->values) {
            mcx_log(LOG_DEBUG, "Results: Store ports: No memory for port values");
            return RETURN_ERROR;
        }
    }

    /* no incrementing numValues, thus no reallocating */

    /* set time */
    retVal = ChannelStorageSetValueFromReferenceAt(channelStore, 0, 0, &time);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Results: Store ports: Could not set time %f", time);
        return RETURN_ERROR;
    }

    /* set channels */
    for (i = 1; i < channels->Size(channels); i++) {
        Channel * channel = (Channel *) channels->At(channels, i);
        retVal = ChannelStorageSetValueFromReferenceAt(channelStore, 0, i, channel->GetValueReference(channel));
        if (RETURN_OK != retVal) { /* error msg in ChannelStorageSetValueFromReferenceAt */
            ChannelInfo *info = &channel->info;
            mcx_log(LOG_DEBUG, "Results: Error in store port %s", ChannelInfoGetLogName(info));
            return RETURN_ERROR;
        }
    }
    //    ChannelStoragePrint(channelStore);
    channelStore->numValues = 1;

    return RETURN_OK;
}

// TODO: boundary handling
// TODO: save number of columns
static ChannelValue ChannelStorageGetValueAt(ChannelStorage * channelStore, size_t row, size_t col) {
    size_t colNum = channelStore->channels->Size(channelStore->channels);
    return channelStore->values[row * colNum + col];
}

static ChannelValue * ChannelStorageGetValuesAtRow(ChannelStorage * channelStore, size_t row) {
    size_t colNum = channelStore->channels->Size(channelStore->channels);
    return channelStore->values + row * colNum;
}


static size_t ChannelStorageLength(ChannelStorage * channelStore) {
    return channelStore->numValues;
}

static ChannelInfo * ChannelStorageGetChannelInfo(ChannelStorage * channelStore, size_t idx) {
    ObjectContainer * channels = channelStore->channels;
    Channel * channel = (Channel *) channels->At(channels, idx);

    if (channel) {
        return &channel->info;
    } else {
        return NULL;
    }
}

static void ChannelStorageDestructor(ChannelStorage * channelStore) {
    if (channelStore) {
        ObjectContainer * channels = channelStore->channels;

        if (channelStore->values) {
            size_t i = 0;
            size_t j = 0;
            for (i = 0; i < channelStore->numValues; i++) {
                size_t colNum = channels->Size(channels);
                for (j = 0; j < colNum; j++) {
                    ChannelValueDestructor(&channelStore->values[i * colNum + j]);
                }
            }
            mcx_free(channelStore->values);
            channelStore->values = NULL;
        }

        if (channels) {
            size_t i = 0;
            if (channels->Size(channels) > 0) {
                Channel * channel = (Channel *) channels->At(channels, 0);
                ChannelInfo * timeInfo = &channel->info;
                object_destroy(timeInfo);
            }
            for (i = 0; i < channels->Size(channels); i++) {
                Channel * channel = (Channel *) channels->At(channels, i);
                object_destroy(channel);
            }
            object_destroy(channels);
        }
    }
}

static ChannelStorage * ChannelStorageCreate(ChannelStorage * channelStore) {
    channelStore->Setup = ChannelStorageSetup;
    channelStore->Store = ChannelStorageStoreFull;

    channelStore->RegisterChannel = ChannelStorageRegisterChannel;
    channelStore->GetChannelNum = ChannelStorageGetChannelNum;

    channelStore->GetValueAt = ChannelStorageGetValueAt;
    channelStore->GetValuesAtRow = ChannelStorageGetValuesAtRow;

    channelStore->Length = ChannelStorageLength;
    channelStore->GetChannelInfo = ChannelStorageGetChannelInfo;

    channelStore->channels = (ObjectContainer *) object_create(ObjectContainer);
    if (!channelStore->channels) {
        return NULL;
    }

    channelStore->numValues = 0;
    channelStore->numValuesAllocated = 0;
    channelStore->values    = NULL;

    channelStore->lastStored = -1.0;

    channelStore->fullStorage = TRUE;

    channelStore->storeCallNum = 0;

    return channelStore;
}

OBJECT_CLASS(ChannelStorage, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */