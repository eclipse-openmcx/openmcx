/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/model/parameters/ScalarParameterInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void ScalarParameterInputDestructor(ScalarParameterInput * input) {
    if (input->name) { mcx_free(input->name); }

    if (input->unit) { mcx_free(input->unit); }

    if (ChannelTypeEq(input->type, &ChannelTypeString) && input->value.defined && input->value.value.s) {
        mcx_free(input->value.value.s);
    }
}

static ScalarParameterInput * ScalarParameterInputCreate(ScalarParameterInput * input) {
    input->name = NULL;

    input->type = &ChannelTypeUnknown;
    OPTIONAL_UNSET(input->value);

    input->unit = NULL;

    return input;
}

OBJECT_CLASS(ScalarParameterInput, InputElement);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */