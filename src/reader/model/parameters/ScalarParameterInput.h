/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_MODEL_PARAMETERS_SCALAR_PARAMETER_INPUT_H
#define MCX_READER_MODEL_PARAMETERS_SCALAR_PARAMETER_INPUT_H

#include "reader/core/InputElement.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const ObjectClass _ScalarParameterInput;

typedef struct ScalarParameterInput {
    InputElement _;

    char * name;

    ChannelType * type;
    OPTIONAL_VALUE(ChannelValueData) value;

    char * unit;

} ScalarParameterInput;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_READER_MODEL_PARAMETERS_SCALAR_PARAMETER_INPUT_H