/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_MODEL_COMPONENTS_COMPONENT_INPUT_H
#define MCX_READER_MODEL_COMPONENTS_COMPONENT_INPUT_H

#include "reader/core/InputElement.h"
#include "reader/model/ports/PortsInput.h"
#include "reader/model/parameters/ParametersInput.h"
#include "reader/model/components/ComponentResultsInput.h"

#include "components/ComponentTypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ComponentInput ComponentInput;

typedef McxStatus (*fComponentInputCopyFrom)(ComponentInput * self, ComponentInput * src);

extern const ObjectClass _ComponentInput;

struct ComponentInput {
    InputElement _;

    ComponentType * type;
    char * name;
    OPTIONAL_VALUE(int) triggerSequence;
    OPTIONAL_VALUE(int) inputAtEndTime;
    OPTIONAL_VALUE(double) deltaTime;

    PortsInput * inports;
    PortsInput * outports;

    ParametersInput * parameters;
    ParametersInput * initialValues;

    ComponentResultsInput * results;

    fComponentInputCopyFrom CopyFrom;
};

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_MODEL_COMPONENTS_COMPONENT_INPUT_H