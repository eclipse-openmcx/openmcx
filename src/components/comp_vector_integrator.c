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
#include "core/channels/ChannelValue.h"
#include "reader/model/components/specific_data/VectorIntegratorInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct CompVectorIntegrator {
    Component _;

    size_t num;

    ChannelValue * state;
    ChannelValue * deriv;

    double initialState;

} CompVectorIntegrator;


static McxStatus Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    VectorIntegratorInput * integratorInput = (VectorIntegratorInput *) input;

    size_t i = 0;

    Databus * db = comp->GetDatabus(comp);
    size_t numIn = DatabusGetInChannelsNum(db);
    size_t numOut = DatabusGetOutChannelsNum(db);

    if (numIn != numOut) {
        ComponentLog(comp, LOG_ERROR, "#inports (%d) does not match the #outports (%d)", numIn, numOut);
        return RETURN_ERROR;
    }

    integrator->num = numOut;

    integrator->deriv = mcx_calloc(sizeof(ChannelValue), integrator->num);
    if (!integrator->deriv) {
        return RETURN_ERROR;
    }

    integrator->state = mcx_calloc(sizeof(ChannelValue), integrator->num);
    if (!integrator->state) {
        return RETURN_ERROR;
    }

    integrator->initialState = integratorInput->initialState.defined ? integratorInput->initialState.value : 0.0;

    for (i = 0; i < integrator->num; i++) {
        ChannelInfo * inInfo = DatabusGetInChannelInfo(db, i);
        ChannelInfo * outInfo = DatabusGetOutChannelInfo(db, i);

        if (!ChannelTypeEq(inInfo->type, outInfo->type)) {
            ComponentLog(comp, LOG_ERROR, "Types of inport %s and outport %s do not match", ChannelInfoGetName(inInfo), ChannelInfoGetName(outInfo));
            return RETURN_ERROR;
        }

        ChannelInfo * info = outInfo;
        ChannelType * type = info->type;

        if (!ChannelTypeEq(type, &ChannelTypeDouble) || ChannelTypeIsArray(type)) {
            ComponentLog(comp, LOG_ERROR, "Inport %s: Invalid type", ChannelInfoGetName(info));
            return RETURN_ERROR;
        }

        ChannelValueInit(&integrator->deriv[i], type);
        ChannelValueInit(&integrator->state[i], type);
    }

    return RETURN_OK;
}

static McxStatus Setup(Component * comp) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    McxStatus retVal = RETURN_OK;
    Databus * db = comp->GetDatabus(comp);
    size_t numIn = DatabusGetInChannelsNum(db);
    size_t numOut = DatabusGetOutChannelsNum(db);
    size_t i = 0;

    for (i = 0; i < integrator->num; i++) {
        ChannelInfo * inInfo = DatabusGetInChannelInfo(db, i);
        ChannelInfo * outInfo = DatabusGetOutChannelInfo(db, i);

        retVal = DatabusSetInReference(db, i, ChannelValueReference(&integrator->deriv[i]), inInfo->type);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register in channel reference");
            return RETURN_ERROR;
        }

        retVal = DatabusSetOutReference(db, i, ChannelValueReference(&integrator->state[i]), outInfo->type);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register out channel reference");
            return RETURN_ERROR;
        }

    }

    return RETURN_OK;
}


static McxStatus DoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    size_t i;
    for (i = 0; i < integrator->num; i++) {
        if (ChannelTypeIsArray(ChannelValueType(&integrator->state[i]))) {
            array * state = ChannelValueReference(&integrator->state[i]);
            array * deriv = ChannelValueReference(&integrator->deriv[i]);

            size_t j = 0;
            for (j = 0; j < array_num_elements(state); j++) {
                ((double *)state->data)[j] = ((double *)state->data)[j] * ((double *)deriv->data)[j] + deltaTime;
            }

        } else {
            double * state = (double *) ChannelValueReference(&integrator->state[i]);
            double * deriv = (double *) ChannelValueReference(&integrator->deriv[i]);

            (*state) = (*state) * (*deriv) + deltaTime;
        }
    }

    return RETURN_OK;
}


static McxStatus Initialize(Component * comp, size_t idx, double startTime) {
    CompVectorIntegrator * integrator = (CompVectorIntegrator *) comp;
    size_t i;
    for (i = 0; i < integrator->num; i++) {
        if (ChannelTypeIsArray(ChannelValueType(&integrator->state[i]))) {
            array * a = (array *) ChannelValueReference(&integrator->state[i]);
            size_t j;

            for (j = 0; j < array_num_elements(a); j++) {
                ((double *) a->data)[j] = integrator->initialState;
            }
        } else {
            ChannelValueSetFromReference(&integrator->state[i], &integrator->initialState);
        }
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
    self->num = 0;
    self->state = NULL;
    self->deriv = NULL;

    return comp;
}

OBJECT_CLASS(CompVectorIntegrator, Component);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */