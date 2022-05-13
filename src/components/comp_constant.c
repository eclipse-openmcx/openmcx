/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "components/comp_constant.h"

#include "core/channels/ChannelValue.h"
#include "core/Databus.h"
#include "reader/model/components/specific_data/ConstantInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    CompConstant * compConstant = (CompConstant *)comp;
    ConstantInput * constantInput = (ConstantInput *)input;

    Databus * db = comp->GetDatabus(comp);
    size_t numOut = DatabusGetOutChannelsNum(db);

    compConstant->values = (ChannelValue **)mcx_calloc(numOut, sizeof(ChannelValue *));

    if (constantInput->values) {
        ConstantValuesInput * values = constantInput->values;
        size_t i = 0;

        for (i = 0; i < numOut; i++) {
            ConstantValueInput * value = (ConstantValueInput *)values->values->At(values->values, i);
            if (value->type == CONSTANT_VALUE_SCALAR) {
                compConstant->values[i] = ChannelValueNewScalar(value->value.scalar->type, &value->value.scalar->value);
                if (!compConstant->values[i]) {
                    ComponentLog(comp, LOG_ERROR, "Could not set channel value");
                    return RETURN_ERROR;
                }
            } else {
                compConstant->values[i] = ChannelValueNewArray(1, &value->value.array->numValues, value->value.array->type, value->value.array->values);
                if (!compConstant->values[i]) {
                    ComponentLog(comp, LOG_ERROR, "Could not set channel value");
                    return RETURN_ERROR;
                }
            }
        }
    }

    return RETURN_OK;
}

static ChannelValue * GetValue(CompConstant * compConstant, size_t idx) {
    Component * comp = (Component *) (compConstant);
    Databus * db = comp->GetDatabus(comp);
    size_t numOut = DatabusGetOutChannelsNum(db);
    ChannelValue * value = NULL;

    if (idx >= numOut) {
        ComponentLog(comp, LOG_ERROR, "GetValue: Invalid index (%d) provided", idx);
        return NULL;
    }

    value = compConstant->values[idx];

    return value;
}

static McxStatus Setup(Component * comp) {
    CompConstant * constComp = (CompConstant *)comp;
    McxStatus retVal = RETURN_OK;
    Databus * db = comp->GetDatabus(comp);
    size_t numOut = DatabusGetOutChannelsNum(db);

    size_t i = 0;

    for (i = 0; i < numOut; i++) {
        retVal = DatabusSetOutReference(db, i, (void *) &constComp->values[i]->value, constComp->values[i]->type);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register out channel reference");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus Initialize(Component * comp, size_t idx, double startTime) {
    return RETURN_OK;
}

static ComponentFinishState CompConstantGetFinishState(const Component * comp) {
    return COMP_NEVER_FINISHES;
}

static void CompConstantDestructor(CompConstant * compConst) {
    Component * comp = (Component *)compConst;
    McxStatus retVal = RETURN_OK;
    Databus * db = comp->GetDatabus(comp);
    size_t numOut = DatabusGetOutChannelsNum(db);

    if (numOut > 0 && compConst->values != NULL) {
        size_t i;
        for (i = 0; i < numOut; i++) {
            ChannelValueDestructor(compConst->values[i]);
            mcx_free(compConst->values[i]);
        }
        mcx_free(compConst->values);
    }
}

static Component * CompConstantCreate(Component * comp) {
    CompConstant * self = (CompConstant *)comp;

    // map to local functions
    comp->Read = Read;
    comp->Setup = Setup;
    comp->Initialize = Initialize;

    comp->GetFinishState = CompConstantGetFinishState;

    // local functions
    self->GetValue = GetValue;
    // local values
    self->values = NULL;

    return comp;
}

OBJECT_CLASS(CompConstant, Component);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */