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


void ChannelValueRefDestructor(ChannelValueRef * ref) {
    // TODO ???
}

ChannelValueRef * ChannelValueRefCreate(ChannelValueRef * ref) {
    ref->type = CHANNEL_VALUE_REF_VALUE;
    ref->ref.value = NULL;

    return ref;
}

OBJECT_CLASS(ChannelValueRef, Object);


McxStatus ChannelValueRefSetFromReference(ChannelValueRef * ref, const void * reference, TypeConversion * conv) {
    if (ref->type == CHANNEL_VALUE_REF_VALUE) {
        if (conv) {
            return conv->Convert(conv, ref, reference);
        }

        return ChannelValueDataSetFromReference(&ref->ref.value->value, ref->ref.value->type, reference);
    } else {
        // slice

        // TODO

        if (conv) {
            return conv->Convert(conv, ref, reference);
        }

        return ChannelValueDataSetFromReference(&ref->ref.slice->ref, ref->ref.value->type, reference);
    }

    return RETURN_OK;
}

McxStatus ChannelValueRefElemMap(ChannelValueRef * ref, fChannelValueRefElemMapFunc fn, void * ctx) {
    switch (ref->type) {
        case CHANNEL_VALUE_REF_VALUE:
            if (ChannelTypeIsArray(ref->ref.value->type)) {
                size_t i = 0;

                for (i = 0; i < mcx_array_num_elements(&ref->ref.value->value.a); i++) {
                    void * elem = mcx_array_get_elem_reference(&ref->ref.value->value.a, i);
                    if (!elem) {
                        return RETURN_ERROR;
                    }

                    if (RETURN_ERROR == fn(elem, ChannelValueType(ref->ref.value), ctx)) {
                        return RETURN_ERROR;
                    }
                }

                return RETURN_OK;
            } else {
                return fn(ChannelValueReference(ref->ref.value), ChannelValueType(ref->ref.value), ctx);
            }
        case CHANNEL_VALUE_REF_SLICE:
            if (ChannelTypeIsArray(ref->ref.slice->ref->type)) {
                size_t i = 0;

                for (i = 0; i < ChannelDimensionNumElements(ref->ref.slice->dimension); i++) {
                    size_t idx = ChannelDimensionGetIndex(ref->ref.slice->dimension, i, ref->ref.slice->ref->value.a.dims);

                    void * elem = mcx_array_get_elem_reference(&ref->ref.slice->ref->value.a, idx);
                    if (!elem) {
                        return RETURN_ERROR;
                    }

                    if (RETURN_ERROR == fn(elem, ChannelValueType(ref->ref.slice->ref), ctx)) {
                        return RETURN_ERROR;
                    }
                }

                return RETURN_OK;
            } else {
                return fn(ChannelValueReference(ref->ref.slice->ref), ChannelValueType(ref->ref.slice->ref), ctx);
            }
        default:
            mcx_log(LOG_ERROR, "ChannelValueRefElemMap: Invalid internal channel value reference type (%d)", ref->type);
            return RETURN_ERROR;
    }
}

ChannelType * ChannelValueRefGetType(ChannelValueRef * ref) {
    switch (ref->type) {
        case CHANNEL_VALUE_REF_VALUE:
            return ChannelValueType(ref->ref.value);
        case CHANNEL_VALUE_REF_SLICE:
            mcx_log(LOG_ERROR, "TODO - change the dimension in ArraySlice to a type and return that to avoid memory allocation");
            return NULL;
        default:
            mcx_log(LOG_ERROR, "Invalid internal channel value reference type (%d)", ref->type);
            return NULL;
    }
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */