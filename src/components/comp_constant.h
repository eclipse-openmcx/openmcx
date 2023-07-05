/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_COMPONENTS_COMP_CONSTANT_H
#define MCX_COMPONENTS_COMP_CONSTANT_H

#include "core/Component.h"
#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct CompConstant CompConstant;

extern const struct ObjectClass _CompConstant;

typedef ChannelValue * (*fCompConstantGetValue)(CompConstant * compConstant, size_t idx);

struct CompConstant {
    Component _;

    fCompConstantGetValue GetValue;

    ChannelValue ** values;
};


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_COMPONENTS_COMP_CONSTANT_H */