/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_DCP_INPUT_H
#define MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_DCP_INPUT_H

#include "reader/core/InputElement.h"
#include "reader/model/components/ComponentInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum DcpSlaveOperationMode {
    SLAVE_OP_MODE_NRT,
    SLAVE_OP_MODE_HRT,
    SLAVE_OP_MODE_SRT
} DcpSlaveOperationMode;

extern const ObjectClass _DcpInput;

typedef struct DcpInput {
    ComponentInput _;

    char * slaveDescPath;

    char * ip;
    OPTIONAL_VALUE(int) port;

    OPTIONAL_VALUE(size_t) numerator;
    OPTIONAL_VALUE(size_t) denominator;

    DcpSlaveOperationMode mode;
} DcpInput;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_MODEL_COMPONENTS_SPECIFIC_DATA_DCP_INPUT_H