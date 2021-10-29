/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/channels/ChannelDimension.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void ChannelDimensionDestructor(ChannelDimension * dimension) {
    if (dimension->startIdxs) { mcx_free(dimension->startIdxs); }
    if (dimension->endIdxs) { mcx_free(dimension->endIdxs); }
}

ChannelDimension * ChannelDimensionCreate(ChannelDimension * dimension) {
    dimension->num = 0;
    dimension->startIdxs = NULL;
    dimension->endIdxs = NULL;

    return dimension;
}

OBJECT_CLASS(ChannelDimension, Object);

McxStatus ChannelDimensionSetup(ChannelDimension * dimension, size_t num) {
    dimension->num = num;

    dimension->startIdxs = (size_t *) mcx_calloc(dimension->num, sizeof(size_t));
    if (!dimension->startIdxs) {
        goto error_cleanup;
    }
    dimension->endIdxs = (size_t *) mcx_calloc(dimension->num, sizeof(size_t));
    if (!dimension->endIdxs) {
        goto error_cleanup;
    }

    return RETURN_OK;

error_cleanup:
    if (dimension->startIdxs) { mcx_free(dimension->startIdxs); }
    if (dimension->endIdxs) { mcx_free(dimension->endIdxs); }

    return RETURN_ERROR;
}

McxStatus ChannelDimensionSetDimension(ChannelDimension * dimension, size_t num, size_t start, size_t end) {
    if (num > dimension->num) {
        return RETURN_ERROR;
    }

    dimension->startIdxs[num] = start;
    dimension->endIdxs[num] = end;

    return RETURN_OK;
}

size_t ChannelDimensionNumElements(ChannelDimension * dimension) {
    size_t i = 0, n = 1;

    if (dimension->num == 0) {
        return 0;
    }

    for (i = 0; i < dimension->num; i++) {
        n *= (dimension->endIdxs[i] - dimension->startIdxs[i] + 1);
    }

    return n;
}

ChannelDimension * ChannelDimensionClone(ChannelDimension * dimension) {
    ChannelDimension * clone = NULL;
    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    if (!dimension) {
        return NULL;
    }

    clone = (ChannelDimension *) object_create(ChannelDimension);
    if (!clone) {
        mcx_log(LOG_ERROR, "ChannelDimensionClone: Not enough memory");
        return NULL;
    }

    retVal = ChannelDimensionSetup(clone, dimension->num);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Channel dimension setup failed");
        goto cleanup;
    }

    for (i = 0; i < dimension->num; i++) {
        retVal = ChannelDimensionSetDimension(clone, i, dimension->startIdxs[i], dimension->endIdxs[i]);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Channel dimension %zu set failed", i);
            goto cleanup;
        }
    }

    return clone;

cleanup:
    object_destroy(clone);
    return NULL;
}

int ChannelDimensionEq(ChannelDimension * first, ChannelDimension * second) {
    size_t i = 0;

    if (!first && !second) {
        return TRUE;
    } else if (!first || !second) {
        return FALSE;
    }

    if (first->num != second->num) {
        return FALSE;
    }

    for (i = 0; i < first->num; i++) {
        if (first->startIdxs[i] != second->startIdxs[i] || first->endIdxs[i] != second->endIdxs[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */