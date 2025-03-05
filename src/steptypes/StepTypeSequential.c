/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "steptypes/StepTypeSequential.h"
#include "core/Component.h"
#include "core/SubModel.h"
#include "core/Databus.h"
#include "storage/ComponentStorage.h"
#include "storage/ResultsStorage.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus CompDoStepAndEnterCommunicationPoint(CompAndGroup * compGroup, void * param) {
    StepTypeParams * params = (StepTypeParams *) param;
    Component * comp = compGroup->comp;

    McxStatus retVal = RETURN_OK;

    TimeInterval interval = {params->timeEndStep, params->timeEndStep};

    retVal = ComponentDoCommunicationStep(compGroup->comp, compGroup->group, params);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Simulation: Element DoStep failed");
        return RETURN_ERROR;
    }

    retVal = ComponentEnterCommunicationPoint(compGroup->comp, &interval);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Simulation: Element EnterCommunicationPoint failed");
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

McxStatus CompPreDoUpdateState(Component * comp, void * param) {
    const StepTypeParams * params = (const StepTypeParams *) param;
    McxStatus retVal = RETURN_OK;

    if (comp->PreDoUpdateState) {
        double time = comp->GetTime(comp);
        double deltaTime = params->timeStepSize;

        TimeInterval interval = {params->time, params->timeEndStep};

        // TODO: clean up the params vs component time logic
        if (comp->HasOwnTime(comp)) {
            interval.startTime = time;

            if (deltaTime > 0.0) {
                interval.endTime = interval.startTime + deltaTime;
            }
        }

        retVal = DatabusTriggerConnectedInConnections(comp->GetDatabus(comp), &interval);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Simulation: Update inports for element PreDoUpdateState failed");
            return RETURN_ERROR;
        }

        retVal = comp->PreDoUpdateState(comp, interval.startTime, deltaTime);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Simulation: Element PreDoUpdateState failed");
            return RETURN_ERROR;
        }
    }

    return retVal;
}

McxStatus CompPostDoUpdateState(Component * comp, void * param) {
    const StepTypeParams * params = (const StepTypeParams *) param;
    McxStatus retVal = RETURN_OK;

    if (comp->PostDoUpdateState) {
        double time = comp->GetTime(comp);
        double deltaTime = params->timeStepSize;

        TimeInterval interval = {params->time, params->timeEndStep};

        // TODO: clean up the params vs component time logic
        if (comp->HasOwnTime(comp)) {
            interval.startTime = time;

            if (deltaTime > 0.0) {
                interval.endTime = interval.startTime + deltaTime;
            }
        }

        retVal = DatabusTriggerConnectedInConnections(comp->GetDatabus(comp), &interval);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Simulation: Update inports for element PostDoUpdateState failed");
            return RETURN_ERROR;
        }

        retVal = comp->PostDoUpdateState(comp, interval.startTime, deltaTime);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Simulation: Element PostDoUpdateState failed");
            return RETURN_ERROR;
        }
    }

    return retVal;
}

static McxStatus SequentialDoStep(StepType * stepType, StepTypeParams * params, SubModel * subModel) {
    McxStatus retVal = RETURN_OK;

    retVal = subModel->LoopComponents(subModel, CompEnterCouplingStepMode, (void *) params);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Simulation: Enter coupling step mode of elements failed");
        return RETURN_ERROR;
    }

    // pre do update: parts of do-step that must not be multi-threaded
    retVal = subModel->LoopComponents(subModel, CompPreDoUpdateState, (void *) params);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Simulation: Pre do update of elements failed");
        return RETURN_ERROR;
    }


    retVal = subModel->LoopEvaluationList(subModel, CompDoStepAndEnterCommunicationPoint, (void *) params);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Simulation: Do step of elements failed");
        return RETURN_ERROR;
    }

    retVal = subModel->LoopComponents(subModel, CompPostDoUpdateState, (void *) params);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Simulation: Post do update of elements failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void StepTypeSequentialDestructor(StepTypeSequential * sequentialType) {

}

static StepTypeSequential * StepTypeSequentialCreate(StepTypeSequential * sequentialType) {
    StepType * type = (StepType *) sequentialType;

    type->DoStep = SequentialDoStep;

    type->type = STEP_TYPE_SEQUENTIAL;

    return sequentialType;
}

OBJECT_CLASS(StepTypeSequential, StepType);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */