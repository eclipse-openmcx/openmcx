/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_INTEGRATOR_INPUT_H
#define MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_INTEGRATOR_INPUT_H

#include "reader/model/components/ComponentInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const ObjectClass _IntegratorInput;

typedef struct IntegratorInput {
    ComponentInput _;

    OPTIONAL_VALUE(double) gain;
    OPTIONAL_VALUE(int64_t) numSubSteps;
    OPTIONAL_VALUE(double) initialState;
} IntegratorInput;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_INTEGRATOR_INPUT_H