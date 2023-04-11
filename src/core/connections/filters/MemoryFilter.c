/********************************************************************************
 * Copyright (c) 2022 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/connections/filters/MemoryFilter.h"

#include "util/compare.h"

#include <limits.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) (-1))
#endif // !SIZE_MAX


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus MemoryFilterSetValue(ChannelFilter * filter, double time, ChannelValueData value) {
    MemoryFilter * memoryFilter = (MemoryFilter *) filter;

    if (InCommunicationMode != * filter->state) {
        if (memoryFilter->numEntriesWrite >= memoryFilter->historySize) {
            mcx_log(LOG_ERROR, "MemoryFilter: History buffer too small");
            return RETURN_ERROR;
        }

        ChannelValueSetFromReference(memoryFilter->valueHistoryWrite + memoryFilter->numEntriesWrite, &value);
        memoryFilter->timeHistoryWrite[memoryFilter->numEntriesWrite] = time;
        memoryFilter->numEntriesWrite++;

#ifdef MCX_DEBUG
        if (time < MCX_DEBUG_LOG_TIME) {
            if (ChannelTypeEq(memoryFilter->valueHistoryWrite[0].type, &ChannelTypeDouble)) {
                MCX_DEBUG_LOG("MemoryFilter: F SET (%x) (%f, %f)", filter, time, value.d);
            } else {
                MCX_DEBUG_LOG("MemoryFilter: F SET (%x) (%f, -)", filter, time);
            }
            MCX_DEBUG_LOG("MemoryFilter: NumEntriesWrite: %zu", memoryFilter->numEntriesWrite);
        }
#endif // MCX_DEBUG
    }

    return RETURN_OK;
}

static ChannelValueData MemoryFilterGetValueReverse(ChannelFilter * filter, double time) {
    MemoryFilter * memoryFilter = (MemoryFilter *) filter;
    size_t i = 0;
    size_t smallerIdx = SIZE_MAX;
    size_t biggerIdx = SIZE_MAX;

    for (i = memoryFilter->numEntriesRead - 1 ; i >= 0; --i) {
        if (double_eq(memoryFilter->timeHistoryRead[i], time)) {
#ifdef MCX_DEBUG
            if (time < MCX_DEBUG_LOG_TIME) {
                if (ChannelTypeEq(memoryFilter->valueHistoryRead[i].type, &ChannelTypeDouble)) {
                    MCX_DEBUG_LOG("MemoryFilter: F GET (%x) (%f, %f)", filter, time, memoryFilter->valueHistoryRead[i].value.d);
                } else {
                    MCX_DEBUG_LOG("MemoryFilter: F GET (%x) (%f, -)", filter, time);
                }
            }
#endif // MCX_DEBUG

            return memoryFilter->valueHistoryRead[i].value;
        }

        // find out closest stored time points
        if (time < memoryFilter->timeHistoryRead[i]) {
            biggerIdx = i;
        } else if (smallerIdx == SIZE_MAX) {
            smallerIdx = i;
        } else {
            break;
        }
    }

    if (smallerIdx == SIZE_MAX) {
        i = biggerIdx;
    } else if (biggerIdx == SIZE_MAX) {
        i = smallerIdx;
    } else if ((time - memoryFilter->timeHistoryRead[smallerIdx]) < (memoryFilter->timeHistoryRead[biggerIdx] - time)) {
        i = smallerIdx;
    } else {
        i = biggerIdx;
    }

#ifdef MCX_DEBUG
    if (time < MCX_DEBUG_LOG_TIME) {
        if (ChannelTypeEq(memoryFilter->valueHistoryRead[i].type, &ChannelTypeDouble)) {
            MCX_DEBUG_LOG("MemoryFilter: F GET CLOSEST (%x) (%f, %f)", filter, time, memoryFilter->valueHistoryRead[i].value.d);
        } else {
            MCX_DEBUG_LOG("MemoryFilter: F GET CLOSEST (%x) (%f, -)", filter, time);
        }
    }
#endif // MCX_DEBUG

    return memoryFilter->valueHistoryRead[i].value;
}

static ChannelValueData MemoryFilterGetValue(ChannelFilter * filter, double time) {
    MemoryFilter * memoryFilter = (MemoryFilter *) filter;
    size_t i = 0;
    size_t smallerIdx = SIZE_MAX;
    size_t biggerIdx = SIZE_MAX;

    for (i = 0; i < memoryFilter->numEntriesRead; ++i) {
        if (double_eq(memoryFilter->timeHistoryRead[i], time)) {
#ifdef MCX_DEBUG
            if (time < MCX_DEBUG_LOG_TIME) {
                if (ChannelTypeEq(memoryFilter->valueHistoryRead[i].type, &ChannelTypeDouble)) {
                    MCX_DEBUG_LOG("MemoryFilter: F GET (%x) (%f, %f)", filter, time, memoryFilter->valueHistoryRead[i].value.d);
                } else {
                    MCX_DEBUG_LOG("MemoryFilter: F GET (%x) (%f, -)", filter, time);
                }
            }
#endif // MCX_DEBUG

            return memoryFilter->valueHistoryRead[i].value;
        }

        // find out closest stored time points
        if (time > memoryFilter->timeHistoryRead[i]) {
            smallerIdx = i;
        } else if (biggerIdx == SIZE_MAX) {
            biggerIdx = i;
        } else {
            break;
        }
    }

    if (smallerIdx == SIZE_MAX) {
        i = biggerIdx;
    } else if (biggerIdx == SIZE_MAX) {
        i = smallerIdx;
    } else if ((time - memoryFilter->timeHistoryRead[smallerIdx]) < (memoryFilter->timeHistoryRead[biggerIdx] - time)) {
        i = smallerIdx;
    } else {
        i = biggerIdx;
    }

#ifdef MCX_DEBUG
    if (time < MCX_DEBUG_LOG_TIME) {
        if (ChannelTypeEq(memoryFilter->valueHistoryRead[i].type, &ChannelTypeDouble)) {
            MCX_DEBUG_LOG("MemoryFilter: F GET CLOSEST (%x) (%f, %f)", filter, time, memoryFilter->valueHistoryRead[i].value.d);
        } else {
            MCX_DEBUG_LOG("MemoryFilter: F GET CLOSEST (%x) (%f, -)", filter, time);
        }
    }
#endif // MCX_DEBUG

    return memoryFilter->valueHistoryRead[i].value;
}

static McxStatus MemoryFilterEnterCouplingStepMode(ChannelFilter * filter,
                                                   double communicationTimeStepSize,
                                                   double sourceTimeStepSize,
                                                   double targetTimeStepSize) {
    MemoryFilter * memoryFilter = (MemoryFilter *) filter;

    MCX_DEBUG_LOG("MemoryFilter: Enter coupling mode (%x): NumEntriesRead: %d, NumEntriesWrite: %d",
                  filter, memoryFilter->numEntriesRead, memoryFilter->numEntriesWrite);

    return RETURN_OK;
}

static McxStatus MemoryFilterEnterCommunicationMode(ChannelFilter * filter, double _time) {
    MemoryFilter* memoryFilter = (MemoryFilter*)filter;

    MCX_DEBUG_LOG("MemoryFilter: Enter synchronization mode (%x): NumEntriesRead: %d, NumEntriesWrite: %d",
                  filter, memoryFilter->numEntriesRead, memoryFilter->numEntriesWrite);

    if (memoryFilter->numEntriesWrite > 0) {
        size_t i = 0;
        if (memoryFilter->numEntriesRead > 1) {
            ChannelValueSetFromReference(&memoryFilter->valueHistoryRead[0], &memoryFilter->valueHistoryRead[memoryFilter->numEntriesRead - 1].value);
            memoryFilter->timeHistoryRead[0] = memoryFilter->timeHistoryRead[memoryFilter->numEntriesRead - 1];
            memoryFilter->numEntriesRead = 1;
        }

        for (i = 0; i < memoryFilter->numEntriesWrite; i++) {
#ifdef MCX_DEBUG
            if (i + memoryFilter->numEntriesRead >= memoryFilter->historySize) {
                mcx_log(LOG_ERROR, "MemoryFilter: Trying to write outside of allocated buffer "
                                   "(HistorySize: %zu, NumEntriesRead: %zu, NumEntriesWrite: %zu",
                        memoryFilter->historySize, memoryFilter->numEntriesRead, memoryFilter->numEntriesWrite);
                return RETURN_ERROR;
            }
#endif // MCX_DEBUG
            ChannelValueSetFromReference(&memoryFilter->valueHistoryRead[i + memoryFilter->numEntriesRead], &memoryFilter->valueHistoryWrite[i].value);
            memoryFilter->timeHistoryRead[i + memoryFilter->numEntriesRead] = memoryFilter->timeHistoryWrite[i];
        }

        memoryFilter->numEntriesRead += memoryFilter->numEntriesWrite;
        memoryFilter->numEntriesWrite = 0;
    }

    return RETURN_OK;
}

static McxStatus MemoryFilterSetup(MemoryFilter * filter, const ChannelType * type, size_t historySize, int reverseSearch) {
    ChannelFilter * channelFilter = (ChannelFilter *)filter;
    size_t i = 0;

    channelFilter->GetValue = reverseSearch ? MemoryFilterGetValueReverse : MemoryFilterGetValue;

    filter->historySize = historySize;
    filter->numEntriesRead = 0;
    filter->numEntriesWrite = 0;

    filter->valueHistoryRead = (ChannelValue *) mcx_malloc(filter->historySize * sizeof(ChannelValue));
    if (!filter->valueHistoryRead) {
        mcx_log(LOG_ERROR, "MemoryFilterSetup: No memory for value buffer (read)");
        goto cleanup;
    }

    filter->valueHistoryWrite = (ChannelValue*)mcx_malloc(filter->historySize * sizeof(ChannelValue));
    if (!filter->valueHistoryWrite) {
        mcx_log(LOG_ERROR, "MemoryFilterSetup: No memory for value buffer (write)");
        goto cleanup;
    }

    filter->timeHistoryRead = (double *) mcx_calloc(filter->historySize, sizeof(double));
    if (!filter->timeHistoryRead) {
        mcx_log(LOG_ERROR, "MemoryFilterSetup: No memory for time buffer (read)");
        goto cleanup;
    }

    filter->timeHistoryWrite = (double*)mcx_calloc(filter->historySize, sizeof(double));
    if (!filter->timeHistoryWrite) {
        mcx_log(LOG_ERROR, "MemoryFilterSetup: No memory for time buffer (write)");
        goto cleanup;
    }

    for (i = 0; i < filter->historySize; i++) {
        ChannelValueInit(filter->valueHistoryRead + i, ChannelTypeClone(type));
    }

    for (i = 0; i < filter->historySize; i++) {
        ChannelValueInit(filter->valueHistoryWrite + i, ChannelTypeClone(type));
    }

    return RETURN_OK;

cleanup:
    if (filter->valueHistoryRead) {
        mcx_free(filter->valueHistoryRead);
    }

    if (filter->valueHistoryWrite) {
        mcx_free(filter->valueHistoryWrite);
    }

    if (filter->timeHistoryRead) {
        mcx_free(filter->timeHistoryRead);
    }

    if (filter->timeHistoryWrite) {
        mcx_free(filter->timeHistoryWrite);
    }

    return RETURN_ERROR;
}

static void MemoryFilterDestructor(MemoryFilter * filter) {
    if (filter->valueHistoryRead) {
        size_t i = 0;

        for (i = 0; i < filter->numEntriesRead; i++) {
            ChannelValueDestructor(&filter->valueHistoryRead[i]);
        }

        mcx_free(filter->valueHistoryRead);
    }

    if (filter->timeHistoryRead) {
        mcx_free(filter->timeHistoryRead);
    }

    if (filter->valueHistoryWrite) {
        size_t i = 0;

        for (i = 0; i < filter->numEntriesWrite; i++) {
            ChannelValueDestructor(&filter->valueHistoryWrite[i]);
        }

        mcx_free(filter->valueHistoryWrite);
    }

    if (filter->timeHistoryWrite) {
        mcx_free(filter->timeHistoryWrite);
    }
}

static MemoryFilter * MemoryFilterCreate(MemoryFilter * memoryFilter) {
    ChannelFilter * filter = (ChannelFilter *) memoryFilter;

    filter->SetValue = MemoryFilterSetValue;
    filter->GetValue = NULL;

    filter->EnterCommunicationMode = MemoryFilterEnterCommunicationMode;
    filter->EnterCouplingStepMode = MemoryFilterEnterCouplingStepMode;

    memoryFilter->Setup = MemoryFilterSetup;

    memoryFilter->valueHistoryRead = NULL;
    memoryFilter->valueHistoryWrite = NULL;
    memoryFilter->timeHistoryRead = NULL;
    memoryFilter->timeHistoryWrite = NULL;

    memoryFilter->numEntriesRead = 0;
    memoryFilter->numEntriesWrite = 0;

    memoryFilter->historySize = 0;

    return memoryFilter;
}

OBJECT_CLASS(MemoryFilter, ChannelFilter);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */