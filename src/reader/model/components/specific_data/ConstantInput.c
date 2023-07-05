/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/model/components/specific_data/ConstantInput.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void ScalarConstantValueInputDestructor(ScalarConstantValueInput * input) {
    ChannelValueDataDestructor(&input->value, input->type);
}

static ScalarConstantValueInput * ScalarConstantValueInputCreate(ScalarConstantValueInput * input) {
    input->type = &ChannelTypeUnknown;
    input->value.d = 0.0;

    return input;
}

OBJECT_CLASS(ScalarConstantValueInput, InputElement);

static void ArrayConstantValueInputDestructor(ArrayConstantValueInput * input) {
    if (input->values) { mcx_free(input->values); }
}

static ArrayConstantValueInput * ArrayConstantValueInputCreate(ArrayConstantValueInput * input) {
    input->type = &ChannelTypeUnknown;

    input->numValues = 0;
    input->values = NULL;

    return input;
}

OBJECT_CLASS(ArrayConstantValueInput, InputElement);

static void ConstantValueInputDestructor(ConstantValueInput * input) {
    if (input->value.scalar) { object_destroy(input->value.scalar); }
}

static ConstantValueInput * ConstantValueInputCreate(ConstantValueInput * input) {
    input->type = CONSTANT_VALUE_SCALAR;
    input->value.scalar = NULL;

    return input;
}

OBJECT_CLASS(ConstantValueInput, InputElement);

static void ConstantValuesInputDestructor(ConstantValuesInput * input) {
    if (input->values) {
        input->values->DestroyObjects(input->values);
        object_destroy(input->values);
    }
}

static ConstantValuesInput * ConstantValuesInputCreate(ConstantValuesInput * input) {
    input->values = (ObjectContainer *) object_create(ObjectContainer);
    if (!input->values) {
        return NULL;
    }

    return input;
}

OBJECT_CLASS(ConstantValuesInput, InputElement);

static void ConstantInputDestructor(ConstantInput * input) {
    if (input->values) { object_destroy(input->values); }
}

static ConstantInput * ConstantInputCreate(ConstantInput * input) {
    input->values = NULL;

    return input;
}

OBJECT_CLASS(ConstantInput, ComponentInput);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */