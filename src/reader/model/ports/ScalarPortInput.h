/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_MODEL_PORTS_SCALAR_PORT_INPUT_H
#define MCX_READER_MODEL_PORTS_SCALAR_PORT_INPUT_H

#include "reader/core/InputElement.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ScalarPortInput ScalarPortInput;

typedef McxStatus (*fScalarPortInputCopyFrom)(ScalarPortInput * self, ScalarPortInput * src);

extern const ObjectClass _ScalarPortInput;

struct ScalarPortInput {
    InputElement _;

    char * name;
    char * nameInModel;
    char * description;
    char * id;
    char * unit;

    ChannelType * type;

    OPTIONAL_VALUE(ChannelValueData) min;
    OPTIONAL_VALUE(ChannelValueData) max;

    OPTIONAL_VALUE(ChannelValueData) scale;
    OPTIONAL_VALUE(ChannelValueData) offset;

    OPTIONAL_VALUE(ChannelValueData) default_;
    OPTIONAL_VALUE(ChannelValueData) initial;

    OPTIONAL_VALUE(int) writeResults;

    fScalarPortInputCopyFrom CopyFrom;
};

void ScalarPortInputPrint(ScalarPortInput * input);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_MODEL_PORTS_SCALAR_PORT_INPUT_H