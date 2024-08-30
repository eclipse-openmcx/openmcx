/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "components/ComponentFactory.h"

#include "CentralParts.h"
#include "core/Component_impl.h"  // friend include
#include "util/string.h"


#include "components/comp_constant.h"
#include "components/comp_fmu.h"
#include "components/comp_integrator.h"
#include "components/comp_vector_integrator.h"
#include "components/comp_dcp.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static Component * _CreateComponent(ComponentFactory * factory, ComponentType * type, size_t id) {
    Component * comp = NULL;

    if (object_same_type(ComponentTypeFmu, type)) {
        comp = (Component *) object_create(CompFMU);
    } else if (object_same_type(ComponentTypeIntegrator, type)) {
        comp = (Component *) object_create(CompIntegrator);
    } else if (object_same_type(ComponentTypeVectorIntegrator, type)) {
        comp = (Component *) object_create(CompVectorIntegrator);
    } else if (object_same_type(ComponentTypeConstant, type)) {
        comp = (Component *) object_create(CompConstant);
    } else if (object_same_type(ComponentTypeDcp, type)) {
        comp = (Component *) object_create(CompDcp);
    }
    if (comp) {
        comp->data->id = id;

        comp->data->typeString = mcx_string_copy(type->ToString(type));
        if (!comp->data->typeString) {
            return NULL;
        }
    }

    return comp;
}

static Component * CreateComponent(ComponentFactory * factory, ComponentType * type, size_t id) {
    return factory->_CreateComponent(factory, type, id);
}

static void ComponentFactoryDestructor(ComponentFactory * factory) {
}

static ComponentFactory * ComponentFactoryCreate(ComponentFactory * factory) {
    factory->CreateComponent = CreateComponent;
    factory->_CreateComponent = _CreateComponent;

    return factory;
}


OBJECT_CLASS(ComponentFactory, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */