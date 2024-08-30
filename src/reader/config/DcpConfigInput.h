/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_CONFIG_DCP_CONFIG_INPUT_H
#define MCX_READER_CONFIG_DCP_CONFIG_INPUT_H

#include "reader/core/InputElement.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const ObjectClass _DcpConfigInput;

typedef struct DcpConfigInput {
    InputElement _;

    OPTIONAL_VALUE(size_t) portFrom;
    OPTIONAL_VALUE(size_t) portTo;

    char * masterIp;
} DcpConfigInput;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_CONFIG_DCP_CONFIG_INPUT_H