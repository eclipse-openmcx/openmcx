/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/model/parameters/ArrayParameterInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void ArrayParameterInputDestructor(ArrayParameterInput * input) {
    if (input->name) { mcx_free(input->name); }
    if (input->unit) { mcx_free(input->unit); }

    if (input->dims) {
        size_t i = 0;

        for (i = 0; i < input->numDims; i++) {
            object_destroy(input->dims[i]);
        }

        mcx_free(input->dims);
    }

    if (input->values) {
        if (ChannelTypeEq(input->type, &ChannelTypeString)) {
            size_t i = 0;

            for (i = 0; i < input->numValues; i++) {
                mcx_free(((char **)input->values)[i]);
            }
        }
        mcx_free(input->values);
    }
}

static ArrayParameterInput * ArrayParameterInputCreate(ArrayParameterInput * input) {
    input->name = NULL;
    input->unit = NULL;

    input->numDims = 0;
    input->dims = NULL;

    input->type = &ChannelTypeUnknown;

    input->numValues = 0;
    input->values = NULL;

    return input;
}

OBJECT_CLASS(ArrayParameterInput, InputElement);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */