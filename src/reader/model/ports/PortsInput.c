/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/model/ports/PortsInput.h"

#include "objects/ObjectContainer.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static InputElement * Clone(InputElement * self) {
    PortsInput * clone = (PortsInput *)object_create(PortsInput);
    McxStatus retVal = RETURN_ERROR;

    if (!clone) {
        return NULL;
    }

    retVal = clone->CopyFrom(clone, (PortsInput *) self);

    // cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(clone);
        return NULL;
    }

    return (InputElement *) clone;
}

static McxStatus CopyFrom(PortsInput * self, PortsInput * src) {
    InputElement * base = (InputElement *)self;
    size_t i = 0;
    McxStatus retVal = RETURN_OK;

    if (!self || !src) {
        mcx_log(LOG_ERROR, "PortsInput: Invalid copy arguments");
        return RETURN_ERROR;
    }

    // copy base class first
    retVal = base->CopyFrom(base, (InputElement *)src);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    // copy src members
    for (i = 0; i < src->ports->Size(src->ports); i++) {
        InputElement * srcPort = (InputElement *) src->ports->At(src->ports, i);
        const char * srcName = src->ports->GetElementName(src->ports, i);

        InputElement * destPort = srcPort->Clone(srcPort);
        if (destPort == NULL) {
            mcx_log(LOG_ERROR, "Failed to clone port %s", srcName);
            return RETURN_ERROR;
        }

        self->ports->PushBackNamed(self->ports, (Object *) destPort, srcName);
    }

    return RETURN_OK;
}

static void PortsInputDestructor(PortsInput * input) {
    if (input->ports) {
        input->ports->DestroyObjects(input->ports);
        object_destroy(input->ports);
    }
}

static PortsInput * PortsInputCreate(PortsInput * input) {
    InputElement * inputElement = (InputElement *)input;

    input->ports = object_create(ObjectContainer);

    if (!input->ports) {
        return NULL;
    }

    inputElement->Clone = Clone;
    input->CopyFrom = CopyFrom;

    return input;
}

OBJECT_CLASS(PortsInput, InputElement);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */