/********************************************************************************
 * Copyright (c) 2023 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_CONFIG_CONFIG_INPUT_H
#define MCX_READER_CONFIG_CONFIG_INPUT_H

#include "reader/core/InputElement.h"

#include "reader/config/DcpConfigInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern const ObjectClass _ConfigInput;

typedef struct ConfigInput {
    InputElement _;

    DcpConfigInput * dcp;

} ConfigInput;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_CONFIG_CONFIG_INPUT_H