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

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */