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

#include "core/Databus.h"
#include "reader/model/components/specific_data/ConstantInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    CompConstant * compConstant = (CompConstant *)comp;
    ConstantInput * constantInput = (ConstantInput *)input;

    Databus * db = comp->GetDatabus(comp);
    size_t numVecOut = DatabusGetOutVectorChannelsNum(db);

    // We read each vector channel (scalars are vectors of length 1)
    // and the corresponding values from the input file.
    compConstant->values = (ChannelValue **)mcx_calloc(numVecOut, sizeof(ChannelValue *));

    if (constantInput->values) {
        ConstantValuesInput * values = constantInput->values;
        size_t i = 0;

        for (i = 0; i < numVecOut; i++) {
            ConstantValueInput * value = (ConstantValueInput *)values->values->At(values->values, i);
            if (value->type == CONSTANT_VALUE_SCALAR) {
                compConstant->values[i] = (ChannelValue *)mcx_calloc(1, sizeof(ChannelValue));
                ChannelValueInit(compConstant->values[i], value->value.scalar->type);
                if (RETURN_OK != ChannelValueSetFromReference(compConstant->values[i], &value->value.scalar->value)) {
                    ComponentLog(comp, LOG_ERROR, "Could not set channel value");
                    return RETURN_ERROR;
                }
            } else {
                size_t j = 0;
                size_t size = ChannelValueTypeSize(value->value.array->type);
                compConstant->values[i] = (ChannelValue *)mcx_calloc(value->value.array->numValues,
                                                                     sizeof(ChannelValue));
                for (j = 0; j < value->value.array->numValues; j++) {
                    ChannelValueInit(compConstant->values[i] + j, value->value.array->type);
                    if (RETURN_OK != ChannelValueSetFromReference(compConstant->values[i] + j,
                                                                  (char *)value->value.array->values + j * size)) {
                        ComponentLog(comp, LOG_ERROR, "Could not set channel value");
                        return RETURN_ERROR;
                    }
                }
            }
        }
    }

    return RETURN_OK;
}

static ChannelValue * GetValue(CompConstant * compConstant, size_t idx) {
    Component * comp = (Component *) (compConstant);
    Databus * db = comp->GetDatabus(comp);
    size_t numVecOut = DatabusGetOutVectorChannelsNum(db);
    size_t i = 0;
    size_t sum = 0;
    ChannelValue * value = NULL;
    VectorChannelInfo * vInfo = NULL;
    size_t numCh = 0;
    size_t startIdx = 0;
    size_t endIdx = 0;

    for (i = 0; i < numVecOut; i++) {
        vInfo = DatabusGetOutVectorChannelInfo(db, i);
        startIdx = vInfo->GetStartIndex(vInfo);
        endIdx = vInfo->GetEndIndex(vInfo);
        numCh = endIdx - startIdx + 1;

        sum += numCh;
        if (sum > idx) {
            break;
        }
    }

    if (i >= numVecOut) {
        ComponentLog(comp, LOG_ERROR, "GetValue: Invalid index (%d) provided", idx);
        return NULL;
    }

    value = compConstant->values[i];
    if (!vInfo->IsScalar(vInfo)) {
        value += idx - (sum - numCh);
    }

    return value;
}

static McxStatus Setup(Component * comp) {
    CompConstant * constComp = (CompConstant *)comp;
    McxStatus retVal = RETURN_OK;
    Databus * db = comp->GetDatabus(comp);
    size_t numVecOut = DatabusGetOutVectorChannelsNum(db);

    size_t i = 0;

    for (i = 0; i < numVecOut; i++) {
        VectorChannelInfo * vInfo = DatabusGetOutVectorChannelInfo(db, i);
        size_t startIdx = vInfo->GetStartIndex(vInfo);
        size_t endIdx = vInfo->GetEndIndex(vInfo);
        size_t numCh = endIdx - startIdx;
        retVal = DatabusSetOutRefVectorChannel(db, i, startIdx, endIdx, constComp->values[i]);
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
    size_t numVecOut = DatabusGetOutVectorChannelsNum(db);

    if (numVecOut > 0 && compConst->values != NULL) {
        size_t i, j;
        for (i = 0; i < numVecOut; i++) {
            VectorChannelInfo * vInfo = DatabusGetOutVectorChannelInfo(db, i);
            size_t startIdx = vInfo->GetStartIndex(vInfo);
            size_t endIdx = vInfo->GetEndIndex(vInfo);
            size_t numCh = endIdx - startIdx + 1;
            for (j = 0; j < numCh; j++) {
                ChannelValueDestructor(&compConst->values[i][j]);
            }
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