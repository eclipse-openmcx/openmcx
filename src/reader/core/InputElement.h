/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_CORE_INPUT_ELEMENT_H
#define MCX_READER_CORE_INPUT_ELEMENT_H

#include "CentralParts.h"

#include "objects/Object.h"

#define MAKE_OPTIONAL_TYPE(T) typedef struct optional_##T {\
    int defined;\
    T value;\
} optional_##T

#define OPTIONAL_VALUE(T) optional_##T

#define MAKE_OPTIONAL_PTR_TYPE(T) typedef struct optional_ptr_##T {\
    int defined;\
    T* value;\
} optional_ptr_##T

#define OPTIONAL_PTR_VALUE(T) optional_ptr_##T

#define OPTIONAL_SET(var_name, val) do {        \
    (var_name).defined = TRUE;                  \
    (var_name).value = val;                     \
    } while (0)

#define OPTIONAL_UNSET(var_name) do {           \
    (var_name).defined = FALSE;                 \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

MAKE_OPTIONAL_TYPE(double);
MAKE_OPTIONAL_TYPE(int);
MAKE_OPTIONAL_TYPE(int64_t);
MAKE_OPTIONAL_TYPE(size_t);
MAKE_OPTIONAL_TYPE(ChannelValueData);
MAKE_OPTIONAL_TYPE(StoreLevel);

typedef enum InputType {
    INPUT_UNSPECIFIED,
    INPUT_SSD,
} InputType;

typedef struct InputElement InputElement;

typedef InputElement * (*fInputElementClone)(InputElement * self);
typedef McxStatus (*fInputElementCopyFrom)(InputElement * self, InputElement * src);

extern const ObjectClass _InputElement;

struct InputElement {
    Object _;

    InputType type;

    void * context;

    fInputElementClone Clone;
    fInputElementCopyFrom CopyFrom;
};

McxStatus input_element_error(InputElement * element, const char * format, ...);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //  MCX_READER_CORE_INPUT_ELEMENT_H