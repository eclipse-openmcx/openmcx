/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_CONSTANT_INPUT_H
#define MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_CONSTANT_INPUT_H

#include "reader/core/InputElement.h"
#include "reader/model/components/ComponentInput.h"

#include "core/channels/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const ObjectClass _ScalarConstantValueInput;

typedef struct ScalarConstantValueInput {
    InputElement _;

    ChannelType * type;
    ChannelValueData value;
} ScalarConstantValueInput;

extern const ObjectClass _ArrayConstantValueInput;

typedef struct ArrayConstantValueInput {
    InputElement _;

    ChannelType * type;

    size_t numValues;
    void * values;
} ArrayConstantValueInput;

typedef enum ConstantValueInputType {
    CONSTANT_VALUE_ARRAY,
    CONSTANT_VALUE_SCALAR
} ConstantValueInputType;

typedef union ConstantValueInputValue {
    ArrayConstantValueInput * array;
    ScalarConstantValueInput * scalar;
} ConstantValueInputValue;

extern const ObjectClass _ConstantValueInput;

typedef struct ConstantValueInput {
    InputElement _;

    ConstantValueInputType type;
    ConstantValueInputValue value;
} ConstantValueInput;

extern const ObjectClass _ConstantValuesInput;

typedef struct ConstantValuesInput {
    InputElement _;

    ObjectContainer * values;
} ConstantValuesInput;

extern const ObjectClass _ConstantInput;

typedef struct ConstantInput {
    ComponentInput _;

    ConstantValuesInput * values;
} ConstantInput;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_CONSTANT_INPUT_H