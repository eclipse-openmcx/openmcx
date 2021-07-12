/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "components/comp_vector_integrator.h"

#include "core/Databus.h"
#include "reader/model/components/specific_data/VectorIntegratorInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct CompVectorIntegrator {
    Component _;

    size_t numStates;
    double * state;
    double * deriv;

    double initialState;

} CompVectorIntegrator;


static McxStatus Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    VectorIntegratorInput * integratorInput = (VectorIntegratorInput *) input;

    size_t numAllIn = 0, numAllOut = 0;
    size_t i = 0;

    Databus * db = comp->GetDatabus(comp);
    size_t numVecIn = DatabusGetInVectorChannelsNum(db);
    size_t numVecOut = DatabusGetOutVectorChannelsNum(db);

    for (i = 0; i < numVecIn; i++) {
        VectorChannelInfo *vInfo = DatabusGetInVectorChannelInfo(db, i);
        size_t numCh = vInfo->GetEndIndex(vInfo) - vInfo->GetStartIndex(vInfo) + 1;
        numAllIn += numCh;
    }
    for (i = 0; i < numVecOut; i++) {
        VectorChannelInfo *vInfo = DatabusGetOutVectorChannelInfo(db, i);
        size_t numCh = vInfo->GetEndIndex(vInfo) - vInfo->GetStartIndex(vInfo) + 1;
        numAllOut += numCh;
    }

    if (numAllIn != numAllOut) {
        ComponentLog(comp, LOG_ERROR, "#inports (%d) does not match the #outports (%d)", numAllIn, numAllOut);
        return RETURN_ERROR;
    }

    integrator->numStates = numAllOut;
    integrator->initialState = integratorInput->initialState.defined ? integratorInput->initialState.value : 0.0;

    return RETURN_OK;
}

static McxStatus Setup(Component * comp) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    McxStatus retVal = RETURN_OK;
    Databus * db = comp->GetDatabus(comp);
    size_t numVecIn = DatabusGetInVectorChannelsNum(db);
    size_t numVecOut = DatabusGetOutVectorChannelsNum(db);
    size_t i = 0;
    size_t nextIdx = 0;

    integrator->deriv = (double *) mcx_malloc(integrator->numStates * sizeof(double));
    integrator->state = (double *) mcx_malloc(integrator->numStates * sizeof(double));

    nextIdx = 0;
    for (i = 0; i < numVecIn; i++) {
        VectorChannelInfo *vInfo = DatabusGetInVectorChannelInfo(db, i);
        size_t startIdx = vInfo->GetStartIndex(vInfo);
        size_t endIdx = vInfo->GetEndIndex(vInfo);
        size_t numCh = endIdx - startIdx;
        retVal = DatabusSetInRefVector(db, i, startIdx, endIdx, integrator->deriv + nextIdx, ChannelTypeDouble);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register in channel reference");
            return RETURN_ERROR;
        }
        nextIdx = nextIdx + numCh + 1;
    }

    nextIdx = 0;
    for (i = 0; i < numVecOut; i++) {
        VectorChannelInfo *vInfo = DatabusGetOutVectorChannelInfo(db, i);
        size_t startIdx = vInfo->GetStartIndex(vInfo);
        size_t endIdx = vInfo->GetEndIndex(vInfo);
        size_t numCh = endIdx - startIdx;
        retVal = DatabusSetOutRefVector(db, i, startIdx, endIdx, integrator->state + nextIdx, ChannelTypeDouble);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register out channel reference");
            return RETURN_ERROR;
        }
        nextIdx = nextIdx + numCh + 1;
    }

    return RETURN_OK;
}


static McxStatus DoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    size_t i;
    for (i = 0; i < integrator->numStates; i++) {
        integrator->state[i] = integrator->state[i] + integrator->deriv[i] * deltaTime;
    }

    return RETURN_OK;
}


static McxStatus Initialize(Component * comp, size_t idx, double startTime) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    size_t i;
    for (i = 0; i < integrator->numStates; i++) {
        integrator->state[i] = integrator->initialState;
    }
    return RETURN_OK;
}

static void CompVectorIntegratorDestructor(CompVectorIntegrator * comp) {
    if (comp->state) {
        mcx_free(comp->state);
    }
    if (comp->deriv) {
        mcx_free(comp->deriv);
    }
}

static Component * CompVectorIntegratorCreate(Component * comp) {
    CompVectorIntegrator * self = (CompVectorIntegrator *) comp;

    // map to local funciotns
    comp->Read = Read;
    comp->Setup = Setup;
    comp->Initialize = Initialize;
    comp->DoStep = DoStep;

    // local values
    self->initialState = 0.;
    self->numStates = 0;
    self->state = NULL;
    self->deriv = NULL;

    return comp;
}

OBJECT_CLASS(CompVectorIntegrator, Component);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */