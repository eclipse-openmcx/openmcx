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
    }

    return RETURN_OK;
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