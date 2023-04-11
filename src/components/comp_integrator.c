/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "components/comp_integrator.h"

#include "core/Databus.h"
#include "reader/model/components/specific_data/IntegratorInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct CompIntegrator {
    Component _;

    double state;
    double gain;
    double deriv;
    double initialState;

    int64_t numSubSteps;
} CompIntegrator;


static McxStatus Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    CompIntegrator * integrator = (CompIntegrator *) comp;
    IntegratorInput * integratorInput = (IntegratorInput *) input;

    if (DatabusGetInChannelsNum(comp->GetDatabus(comp)) != 1) {
        ComponentLog(comp, LOG_ERROR, "Illegal number of input channels");
        ComponentLog(comp, LOG_ERROR, "Expected: %d, Read: %d", 1, DatabusGetInChannelsNum(comp->GetDatabus(comp)));
        return RETURN_ERROR;
    }

    if ((DatabusGetOutChannelsNum(comp->GetDatabus(comp)) < 0) ||  (DatabusGetOutChannelsNum(comp->GetDatabus(comp)) > 1)) {
        ComponentLog(comp, LOG_ERROR, "Illegal number of input channels");
        ComponentLog(comp, LOG_ERROR, "Expected: %d or %d, Read: %d", 0, 1, DatabusGetOutChannelsNum(comp->GetDatabus(comp)));
        return RETURN_ERROR;
    }

    integrator->gain = integratorInput->gain.defined ? integratorInput->gain.value : 1.0;
    integrator->numSubSteps = integratorInput->numSubSteps.defined ? integratorInput->numSubSteps.value : 1;
    integrator->initialState = integratorInput->initialState.defined ? integratorInput->initialState.value : 0.0;

    return RETURN_OK;
}

static McxStatus Setup(Component * comp) {
    CompIntegrator * integrator = (CompIntegrator *) comp;
    McxStatus retVal = RETURN_OK;

    retVal = DatabusSetInReference(comp->GetDatabus(comp), 0, &integrator->deriv, &ChannelTypeDouble);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not register in channel reference");
        return RETURN_ERROR;
    }

    retVal = DatabusSetOutReference(comp->GetDatabus(comp), 0, &integrator->state, &ChannelTypeDouble);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not register out channel reference");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}


static McxStatus DoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep) {
    CompIntegrator * integrator = (CompIntegrator *) comp;

    int i = 0;
    double subDeltaTime = deltaTime / integrator->numSubSteps;
    double currTime = time;

    integrator->state += integrator->gain * integrator->deriv * subDeltaTime;

    for (i = 1; i < integrator->numSubSteps; i++) {
        McxStatus retVal = RETURN_OK;
        TimeInterval timeInterval;

        currTime += subDeltaTime;

        timeInterval.startTime = currTime;
        timeInterval.endTime = currTime;
        retVal = DatabusTriggerInConnections(comp->GetDatabus(comp), &timeInterval);
        if (RETURN_ERROR == retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not update inports at time %g", currTime);
            return RETURN_ERROR;
        }

        integrator->state += integrator->gain * integrator->deriv * subDeltaTime;
    }

    return RETURN_OK;
}


static McxStatus Initialize(Component * comp, size_t idx, double startTime) {
    CompIntegrator * integratorComp = (CompIntegrator *) comp;

    integratorComp->state = integratorComp->initialState;

    return RETURN_OK;
}

static void CompIntegratorDestructor(CompIntegrator * comp) {

}

static Component * CompIntegratorCreate(Component * comp) {
    CompIntegrator * self = (CompIntegrator *) comp;

    // map to local funciotns
    comp->Read = Read;
    comp->Setup = Setup;
    comp->Initialize = Initialize;
    comp->DoStep = DoStep;

    // local values
    self->gain = 1.;
    self->initialState = 0.;
    self->numSubSteps = 1;

    return comp;
}

OBJECT_CLASS(CompIntegrator, Component);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */