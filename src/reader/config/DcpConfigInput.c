/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/config/DcpConfigInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void DcpConfigInputDestructor(DcpConfigInput * input) {
    if (input->masterIp) { mcx_free(input->masterIp); }
}

static DcpConfigInput * DcpConfigInputCreate(DcpConfigInput * input) {
    OPTIONAL_UNSET(input->portFrom);
    OPTIONAL_UNSET(input->portTo);

    input->masterIp = NULL;

    return input;
}

OBJECT_CLASS(DcpConfigInput, InputElement);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */