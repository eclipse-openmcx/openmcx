/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "components/ComponentTypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*****************************************************************************/
/*                        ComponentType                                      */
/*****************************************************************************/
static const char * ComponentTypeToString(ComponentType * type) {
    return NULL;
}

static void ComponentTypeDestructor(ComponentType * type) {
}

static ComponentType * ComponentTypeCreate(ComponentType * type) {
    ComponentType * base = (ComponentType *)type;

    base->ToString = ComponentTypeToString;

    return type;
}

OBJECT_CLASS(ComponentType, Object);


/*****************************************************************************/
/*                        ComponentTypeConstant                               */
/*****************************************************************************/
const char * const compConstantTypeString = "CONSTANT";

static const char * ComponentTypeConstantToString(ComponentType * type) {
    return compConstantTypeString;
}

static void ComponentTypeConstantDestructor(ComponentTypeConstant * type) {
}

static ComponentTypeConstant * ComponentTypeConstantCreate(ComponentTypeConstant * type) {
    ComponentType * base = (ComponentType *)type;

    base->ToString = ComponentTypeConstantToString;

    return type;
}

OBJECT_CLASS(ComponentTypeConstant, ComponentType);

/*****************************************************************************/
/*                        ComponentTypeFmu                                   */
/*****************************************************************************/
static const char * ComponentTypeFmuToString(ComponentType * type) {
    return "FMU";
}

static void ComponentTypeFmuDestructor(ComponentTypeFmu * type) {
}

static ComponentTypeFmu * ComponentTypeFmuCreate(ComponentTypeFmu * type) {
    ComponentType * base = (ComponentType *)type;

    base->ToString = ComponentTypeFmuToString;

    return type;
}

OBJECT_CLASS(ComponentTypeFmu, ComponentType);

/*****************************************************************************/
/*                        ComponentTypeIntegrator                            */
/*****************************************************************************/
static const char * ComponentTypeIntegratorToString(ComponentType * type) {
    return "INTEGRATOR";
}

static void ComponentTypeIntegratorDestructor(ComponentTypeIntegrator * type) {
}

static ComponentTypeIntegrator * ComponentTypeIntegratorCreate(ComponentTypeIntegrator * type) {
    ComponentType * base = (ComponentType *)type;

    base->ToString = ComponentTypeIntegratorToString;

    return type;
}

OBJECT_CLASS(ComponentTypeIntegrator, ComponentType);

/*****************************************************************************/
/*                        ComponentTypeVectorIntegrator                      */
/*****************************************************************************/
static const char * ComponentTypeVectorIntegratorToString(ComponentType * type) {
    return "VECTOR_INTEGRATOR";
}

static void ComponentTypeVectorIntegratorDestructor(ComponentTypeVectorIntegrator * type) {
}

static ComponentTypeVectorIntegrator * ComponentTypeVectorIntegratorCreate(ComponentTypeVectorIntegrator * type) {
    ComponentType * base = (ComponentType *)type;

    base->ToString = ComponentTypeVectorIntegratorToString;

    return type;
}

OBJECT_CLASS(ComponentTypeVectorIntegrator, ComponentType);

/*****************************************************************************/
/*                        ComponentTypeDcp                                   */
/*****************************************************************************/
static const char * ComponentTypeDcpToString(ComponentType * type) {
    return "DCP";
}

static void ComponentTypeDcpDestructor(ComponentTypeDcp * type) {
}

static ComponentTypeDcp * ComponentTypeDcpCreate(ComponentTypeDcp * type) {
    ComponentType * base = (ComponentType *)type;

    base->ToString = ComponentTypeDcpToString;

    return type;
}

OBJECT_CLASS(ComponentTypeDcp, ComponentType);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */