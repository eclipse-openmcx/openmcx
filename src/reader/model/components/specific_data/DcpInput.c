/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/model/components/specific_data/DcpInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static void DcpInputDestructor(DcpInput * input) {
    if (input->ip) { mcx_free(input->ip); }
    if (input->slaveDescPath) { mcx_free(input->slaveDescPath); }
}

static DcpInput * DcpInputCreate(DcpInput * input) {
    input->slaveDescPath = NULL;

    input->ip = NULL;
    OPTIONAL_UNSET(input->port);

    OPTIONAL_UNSET(input->numerator);
    OPTIONAL_UNSET(input->denominator);

    input->mode = SLAVE_OP_MODE_NRT;

    return input;
}

OBJECT_CLASS(DcpInput, ComponentInput);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */