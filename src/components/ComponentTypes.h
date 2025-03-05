/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_COMPONENTS_COMPONENT_TYPES_H
#define MCX_COMPONENTS_COMPONENT_TYPES_H

#include "CentralParts.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ComponentType ComponentType;
typedef const char * (*fComponentTypeToString)(ComponentType * type);

extern const ObjectClass _ComponentType;
struct ComponentType {
    Object _;

    fComponentTypeToString ToString;
};


extern const char * const compConstantTypeString;
extern const ObjectClass _ComponentTypeConstant;
typedef struct ComponentTypeConstant {
    ComponentType _;
} ComponentTypeConstant;

extern const ObjectClass _ComponentTypeFmu;
typedef struct ComponentTypeFmu {
    ComponentType _;
} ComponentTypeFmu;

extern const ObjectClass _ComponentTypeIntegrator;
typedef struct ComponentTypeIntegrator {
    ComponentType _;
} ComponentTypeIntegrator;

extern const ObjectClass _ComponentTypeVectorIntegrator;
typedef struct ComponentTypeVectorIntegrator {
    ComponentType _;
} ComponentTypeVectorIntegrator;


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_COMPONENTS_COMPONENT_TYPES_H */