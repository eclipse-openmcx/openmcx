/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_MODEL_PORTS_VECTOR_PORT_INPUT_H
#define MCX_READER_MODEL_PORTS_VECTOR_PORT_INPUT_H

#include "reader/core/InputElement.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct VectorPortInput VectorPortInput;

typedef McxStatus (*fVectorPortInputCopyFrom)(VectorPortInput * self, VectorPortInput * src);

extern const ObjectClass _VectorPortInput;

struct VectorPortInput {
    InputElement _;

    int startIndex;
    int endIndex;

    char * name;
    char * nameInModel;
    char * description;
    char * id;
    char * unit;

    ChannelType type;

    void * min;
    void * max;

    void * scale;
    void * offset;

    void * default_;
    void * initial;

    OPTIONAL_VALUE(int) writeResults;

    fVectorPortInputCopyFrom CopyFrom;
};

void VectorPortInputPrint(VectorPortInput * input);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_MODEL_PORTS_VECTOR_PORT_INPUT_H