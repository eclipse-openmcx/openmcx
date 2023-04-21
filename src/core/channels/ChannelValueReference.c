/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/channels/ChannelValueReference.h"
#include "common/logging.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void DestroyChannelValueReference(ChannelValueReference * ref) {
    if (ref->type == CHANNEL_VALUE_REF_SLICE) {
        if (ref->ref.slice.dimension) {
            DestroyChannelDimension(ref->ref.slice.dimension);
        }
    }

    mcx_free(ref);
}


ChannelValueReference * MakeChannelValueReference(ChannelValue * value, ChannelDimension * slice) {
    ChannelValueReference * ref = (ChannelValueReference *) mcx_calloc(1, sizeof(ChannelValueReference));

    if (!ref) {
        return NULL;
    }

    if (slice) {
        ref->type = CHANNEL_VALUE_REF_SLICE;
        ref->ref.slice.dimension = slice;
        ref->ref.slice.ref = value;
    } else {
        ref->type = CHANNEL_VALUE_REF_VALUE;
        ref->ref.value = value;
    }

    return ref;
}


McxStatus ChannelValueReferenceSetFromPointer(ChannelValueReference * ref, const void * ptr, ChannelDimension * srcDimension, TypeConversion * conv) {
    if (conv) {
        return conv->Convert(conv, ref, (void *) ptr);
    }

    if (ref->type == CHANNEL_VALUE_REF_VALUE) {
        if (ChannelTypeIsArray(ref->ref.value->type)) {
            mcx_array * destArray = &ref->ref.value->value.a;
            mcx_array * srcArray = (mcx_array *) ptr;

            if (srcArray->data == NULL || destArray->data == NULL) {
                mcx_log(LOG_ERROR, "ChannelValueReferenceSetFromPointer: Empty array data given");
                return RETURN_ERROR;
            }

            if (srcArray->numDims == 1) {
                // if there is only one dimension, values are sequentially stored in memory so we can use memcpy
                // we could also check if for numDims > 1, values are sequentially stored or not, but there is no
                // need for that now
                size_t numBytes = ChannelValueTypeSize(destArray->type) * mcx_array_num_elements(destArray);
                size_t srcOffset = srcDimension != 0 ? srcDimension->startIdxs[0] * ChannelValueTypeSize(srcArray->type) : 0;
                void * sourceData = (char *) srcArray->data + srcOffset;

                memcpy(destArray->data, sourceData, numBytes);
            } else {
                // copy element by element
                size_t i = 0;
                size_t numElems = srcDimension ? ChannelDimensionNumElements(srcDimension) : mcx_array_num_elements(destArray);

                for (i = 0; i < numElems; i++) {
                    size_t srcIdx = srcDimension ? ChannelDimensionGetIndex(srcDimension, i, srcArray->dims) : i;
                    void * srcElem = mcx_array_get_elem_reference(srcArray, srcIdx);
                    void * destElem = mcx_array_get_elem_reference(destArray, i);
                    memcpy(destElem, srcElem, ChannelValueTypeSize(destArray->type));
                }
            }

            return RETURN_OK;
        } else {
            if (srcDimension) {
                mcx_array * src = (mcx_array *) (ptr);

                if (ChannelDimensionNumElements(srcDimension) != 1) {
                    mcx_log(LOG_ERROR, "ChannelValueReferenceSetFromPointer: Setting scalar value from an array");
                    return RETURN_ERROR;
                }

                return ChannelValueDataSetFromReference(
                    &ref->ref.value->value,
                    ref->ref.value->type,
                    mcx_array_get_elem_reference(src, ChannelDimensionGetIndex(srcDimension, 0, src->dims)));
            } else {
                return ChannelValueDataSetFromReference(&ref->ref.value->value, ref->ref.value->type, ptr);
            }
        }
    } else {
        if (ChannelTypeIsArray(ref->ref.slice.ref->type)) {
            mcx_array * destArray = &ref->ref.slice.ref->value.a;
            mcx_array * srcArray = (mcx_array *) ptr;

            ChannelDimension * destDimension = ref->ref.slice.dimension;

            if (srcArray->data == NULL || destArray->data == NULL) {
                mcx_log(LOG_ERROR, "ChannelValueReferenceSetFromPointer: Empty array data given");
                return RETURN_ERROR;
            }

            if (srcArray->numDims == 1) {
                // if there is only one dimension, values are sequentially stored in memory so we can use memcpy
                // we could also check if for numDims > 1, values are sequentially stored or not, but there is no
                // need for that now
                size_t numBytes = ChannelValueTypeSize(destArray->type) * (destDimension->endIdxs[0] - destDimension->startIdxs[0] + 1);
                size_t destOffset = destDimension->startIdxs[0] * ChannelValueTypeSize(destArray->type);
                void * destData = (char *) destArray->data + destOffset;
                size_t srcOffset = srcDimension != 0 ? srcDimension->startIdxs[0] * ChannelValueTypeSize(srcArray->type) : 0;
                void * sourceData = (char *) srcArray->data + srcOffset;

                memcpy(destData, sourceData, numBytes);
            } else {
                // copy element by element
                size_t i = 0;
                size_t numElems = srcDimension ? ChannelDimensionNumElements(srcDimension) : mcx_array_num_elements(destArray);

                for (i = 0; i < numElems; i++) {
                    size_t srcIdx = srcDimension ? ChannelDimensionGetIndex(srcDimension, i, srcArray->dims) : i;
                    size_t destIdx = ChannelDimensionGetIndex(destDimension, i, destArray->dims);

                    void * srcElem = mcx_array_get_elem_reference(srcArray, srcIdx);
                    void * destElem = mcx_array_get_elem_reference(destArray, destIdx);
                    memcpy(destElem, srcElem, ChannelValueTypeSize(destArray->type));
                }
            }

            return RETURN_OK;
        } else {
            if (srcDimension) {
                mcx_array * src = (mcx_array *) (ptr);

                if (ChannelDimensionNumElements(srcDimension) != 1) {
                    mcx_log(LOG_ERROR, "ChannelValueReferenceSetFromPointer: Setting scalar value from an array");
                    return RETURN_ERROR;
                }

                return ChannelValueDataSetFromReference(
                    &ref->ref.slice.ref->value,
                    ref->ref.slice.ref->type,
                    mcx_array_get_elem_reference(src, ChannelDimensionGetIndex(srcDimension, 0, src->dims)));
            } else {
                return ChannelValueDataSetFromReference(&ref->ref.slice.ref->value, ref->ref.slice.ref->type, ptr);
            }
        }
    }

    return RETURN_OK;
}

McxStatus ChannelValueReferenceElemMap(ChannelValueReference * ref, fChannelValueReferenceElemMapFunc fn, void * ctx) {
    switch (ref->type) {
        case CHANNEL_VALUE_REF_VALUE:
            if (ChannelTypeIsArray(ref->ref.value->type)) {
                size_t i = 0;

                for (i = 0; i < mcx_array_num_elements(&ref->ref.value->value.a); i++) {
                    void * elem = mcx_array_get_elem_reference(&ref->ref.value->value.a, i);
                    if (!elem) {
                        return RETURN_ERROR;
                    }

                    if (RETURN_ERROR == fn(elem, i, ChannelValueType(ref->ref.value), ctx)) {
                        return RETURN_ERROR;
                    }
                }

                return RETURN_OK;
            } else {
                return fn(ChannelValueDataPointer(ref->ref.value), 0, ChannelValueType(ref->ref.value), ctx);
            }
        case CHANNEL_VALUE_REF_SLICE:
            if (ChannelTypeIsArray(ref->ref.slice.ref->type)) {
                size_t i = 0;

                for (i = 0; i < ChannelDimensionNumElements(ref->ref.slice.dimension); i++) {
                    size_t idx = ChannelDimensionGetIndex(ref->ref.slice.dimension, i, ref->ref.slice.ref->value.a.dims);

                    void * elem = mcx_array_get_elem_reference(&ref->ref.slice.ref->value.a, idx);
                    if (!elem) {
                        return RETURN_ERROR;
                    }

                    if (RETURN_ERROR == fn(elem, idx, ChannelValueType(ref->ref.slice.ref), ctx)) {
                        return RETURN_ERROR;
                    }
                }

                return RETURN_OK;
            } else {
                return fn(ChannelValueDataPointer(ref->ref.slice.ref), 0, ChannelValueType(ref->ref.slice.ref), ctx);
            }
        default:
            mcx_log(LOG_ERROR, "ChannelValueReferenceElemMap: Invalid internal channel value reference type (%d)", ref->type);
            return RETURN_ERROR;
    }
}

ChannelType * ChannelValueReferenceGetType(ChannelValueReference * ref) {
    switch (ref->type) {
        case CHANNEL_VALUE_REF_VALUE:
            return ChannelValueType(ref->ref.value);
        case CHANNEL_VALUE_REF_SLICE:
            return ChannelValueType(ref->ref.slice.ref);
        default:
            mcx_log(LOG_ERROR, "Invalid internal channel value reference type (%d)", ref->type);
            return NULL;
    }
}



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */