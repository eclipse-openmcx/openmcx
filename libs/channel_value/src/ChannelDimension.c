/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "channel_value/ChannelDimension.h"

#include <stdio.h>

#include "common/logging.h"
#include "common/definitions.h"
#include "common/memory.h"

#include "util/stdlib.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



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

McxStatus ChannelDimensionSetDimension(ChannelDimension * dimension, size_t dim, size_t start, size_t end) {
    if (dim > dimension->num) {
        return RETURN_ERROR;
    }

    dimension->startIdxs[dim] = start;
    dimension->endIdxs[dim] = end;

    return RETURN_OK;
}

ChannelDimension * ChannelDimensionCreate1D(size_t start, size_t end) {
    McxStatus retVal = RETURN_OK;

    ChannelDimension * dimension = MakeChannelDimension();
    if (dimension == NULL) {
        mcx_log(LOG_ERROR, "ChannelDimensionCreate1D: Not enough memory");
        return NULL;
    }

    retVal = ChannelDimensionSetup(dimension, 1);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ChannelDimensionCreate1D: Channel dimension setup failed");
        return NULL;
    }

    retVal = ChannelDimensionSetDimension(dimension, 0, start, end);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ChannelDimensionCreate1D: Setting channel dimension (%zu -> %zu) failed", start, end);
        return NULL;
    }

    return dimension;
}

size_t ChannelDimensionNumElements(const ChannelDimension * dimension) {
    size_t i = 0;
    size_t n = 1;

    if (dimension->num == 0) {
        return 0;
    }

    for (i = 0; i < dimension->num; i++) {
        n *= (dimension->endIdxs[i] - dimension->startIdxs[i] + 1);
    }

    return n;
}

ChannelDimension * CloneChannelDimension(const ChannelDimension * dimension) {
    ChannelDimension * clone = NULL;
    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    if (!dimension) {
        return NULL;
    }

    clone = MakeChannelDimension();
    if (!clone) {
        mcx_log(LOG_ERROR, "CloneChannelDimension: Not enough memory");
        return NULL;
    }

    retVal = ChannelDimensionSetup(clone, dimension->num);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "CloneChannelDimension: Channel dimension setup failed");
        goto cleanup;
    }

    for (i = 0; i < dimension->num; i++) {
        retVal = ChannelDimensionSetDimension(clone, i, dimension->startIdxs[i], dimension->endIdxs[i]);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "CloneChannelDimension: Channel dimension %zu set failed", i);
            goto cleanup;
        }
    }

    return clone;

cleanup:
    DestroyChannelDimension(clone);
    return NULL;
}

int ChannelDimensionEq(const ChannelDimension * first, const ChannelDimension * second) {
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

int ChannelDimensionConformsToDimension(const ChannelDimension * first, const ChannelDimension * second) {
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
        if (first->endIdxs[i] - first->startIdxs[i] != second->endIdxs[i] - second->startIdxs[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

int ChannelDimensionConformsTo(const ChannelDimension * dimension, const size_t * dims, size_t numDims) {
    size_t i = 0;
    if (dimension->num != numDims) {
        return 0;
    }

    for (i = 0; i < numDims; i++) {
        if (dims[i] != (dimension->endIdxs[i] - dimension->startIdxs[i] + 1)) {
            return 0;
        }
    }

    return 1;
}

int ChannelDimensionIncludedIn(const ChannelDimension * first, const ChannelDimension * second) {
    size_t i = 0;

    if (!first && !second) {
        return TRUE;
    } else if (!first || !second) {
        return FALSE;
    }

    if (first->num != second->num) {
        return FALSE;  // only same number of dimensions is comparable
    }

    for (i = 0; i < first->num; i++) {
        if (first->startIdxs[i] < second->startIdxs[i] || first->startIdxs[i] > second->endIdxs[i]) {
            return FALSE;
        }

        if (first->endIdxs[i] > second->endIdxs[i] || first->endIdxs[i] < second->startIdxs[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

size_t ChannelDimensionGetIndex(const ChannelDimension * dimension, size_t elem_idx, const size_t * sizes) {
    switch (dimension->num) {
        case 1:
            {
                size_t idx = dimension->startIdxs[0] + elem_idx;
                if (idx > dimension->endIdxs[0]) {
                    mcx_log(LOG_ERROR, "ChannelDimensionGetIndex: Index out of range");
                    break;
                }

                return idx;
            }
        case 2:
            {
                size_t dim_1_slice_size = dimension->endIdxs[1] - dimension->startIdxs[1] + 1;

                size_t slice_idx_0 = elem_idx / dim_1_slice_size + dimension->startIdxs[0];
                size_t slice_idx_1 = elem_idx % dim_1_slice_size + dimension->startIdxs[1];

                if (slice_idx_0 > dimension->endIdxs[0] || slice_idx_1 > dimension->endIdxs[1]) {
                    mcx_log(LOG_ERROR, "ChannelDimensionGetIndex: Index out of range");
                    break;
                }

                return slice_idx_0 * sizes[1] + slice_idx_1;
            }
        default:
            mcx_log(LOG_ERROR, "ChannelDimensionGetIndex: Number of dimensions not supported (%d)", dimension->num);
            break;
    }

    return (size_t) -1;
}

size_t ChannelDimensionGetSliceIndex(const ChannelDimension * dimension, size_t slice_idx, const size_t * dims) {
    switch (dimension->num) {
        case 1:
            {
                size_t idx = slice_idx - dimension->startIdxs[0];
                if (idx > dimension->endIdxs[0]) {
                    mcx_log(LOG_ERROR, "ChannelDimensionGetSliceIndex: Index out of range");
                    break;
                }

                return idx;
            }
        case 2:
            {
                size_t idx_0 = slice_idx / dims[1];
                size_t idx_1 = slice_idx - idx_0 * dims[1];

                size_t slice_idx_0 = idx_0 - dimension->startIdxs[0];
                size_t slice_idx_1 = idx_1 - dimension->startIdxs[1];

                size_t dim_1_slice_size = dimension->endIdxs[1] - dimension->startIdxs[1] + 1;

                return slice_idx_0 * dim_1_slice_size + slice_idx_1;
            }
        default:
            mcx_log(LOG_ERROR, "ChannelDimensionGetSliceIndex: Number of dimensions not supported (%d)", dimension->num);
            break;
    }

    return (size_t) -1;
}

char * ChannelDimensionString(const ChannelDimension * dimension) {
    char * str = NULL;
    size_t length = 0;
    size_t i = 0;
    int n = 0;

    if (!dimension) {
        return NULL;
    }

    for (i = 0; i < dimension->num; i++) {
        length += 1;                                        // '('
        length += mcx_digits10(dimension->startIdxs[i]);    // a
        length += 2;                                        // ', '
        length += mcx_digits10(dimension->endIdxs[i]);      // b
        length += 1;                                        // ')'
    }

    length += dimension->num - 1;                           // spaces between dimensions
    length += 2;                                            // '[' at the beginning and ']' at the end
    length += 1;                                            // '\0'

    str = (char *) mcx_calloc(sizeof(char), length);
    if (!str) {
        mcx_log(LOG_ERROR, "ChannelDimensionString: Not enough memory");
        return NULL;
    }

    n += sprintf(str, "[");
    for (i = 0; i < dimension->num; i++) {
        if (i > 0) {
            n += sprintf(str + n, " ");
        }
        n += sprintf(str + n, "(%zu, %zu)", dimension->startIdxs[i], dimension->endIdxs[i]);
    }
    sprintf(str + n, "]");

    return str;
}

McxStatus ChannelDimensionAlignIndicesWithZero(ChannelDimension * target, const ChannelDimension * base) {
    size_t i = 0;
    if (!target && !base) {
        return RETURN_OK;
    } else if (!target || !base) {
        return RETURN_ERROR;
    }

    if (target->num != base->num) {
        return RETURN_ERROR;
    }

    for (i = 0; i < target->num; i++) {
        target->endIdxs[i] -= base->startIdxs[i];
        target->startIdxs[i] -= base->startIdxs[i];
    }

    return RETURN_OK;
}

ChannelDimension * MakeChannelDimension() {
    ChannelDimension * dimension = (ChannelDimension *) mcx_calloc(1, sizeof(ChannelDimension));

    return dimension;
}

void DestroyChannelDimension(ChannelDimension * dimension) {
    if (dimension->startIdxs) { mcx_free(dimension->startIdxs); }
    if (dimension->endIdxs) { mcx_free(dimension->endIdxs); }
}



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */