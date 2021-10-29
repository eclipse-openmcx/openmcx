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


McxStatus ChannelValueRefSetFromReference(ChannelValueRef * ref, const void * reference) {
    if (ref->type == CHANNEL_VALUE_REF_VALUE) {
        // value
        return ChannelValueDataSetFromReference(&ref->ref.value->value, ref->ref.value->type, reference);
    } else {
        // slice

        // TODO
    }

    return RETURN_OK;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */