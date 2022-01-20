/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/Component.h"
#include "core/SubModel.h"
#include "core/Databus.h"
#include "storage/ComponentStorage.h"
#include "storage/ResultsStorage.h"
#include "steptypes/StepType.h"
#include "util/compare.h"
#include "util/signals.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Step Type: Common

McxStatus ComponentDoCommunicationStep(Component * comp, size_t group, StepTypeParams * params) {
    McxStatus retVal = RETURN_OK;
    double time = params->time;
    double timeStep = params->timeStepSize;
    double stepEndTime = params->timeEndStep;
    double tmpTime = 0.;

    /* interval of current coupling step */
    TimeInterval interval = {time, stepEndTime};

    StoreLevel level = STORE_SYNCHRONIZATION;

    int numSteps = 0;

    if (comp->GetFinishState(comp) == COMP_IS_FINISHED) {
        params->aComponentFinished = 1;
        return RETURN_OK;
    }

    level = STORE_SYNCHRONIZATION;

    while (
        comp->GetFinishState(comp) != COMP_IS_FINISHED &&
        double_lt(comp->GetTime(comp), stepEndTime)
    ) {
        if (comp->HasOwnTime(comp)) {
            interval.startTime = comp->GetTime(comp);
            if (comp->GetTimeStep(comp) > 0.) {
                timeStep = comp->GetTimeStep(comp);
                interval.endTime = interval.startTime + timeStep;
            } else {
                interval.endTime = params->timeEndStep;
            }
        }

        // TODO: Rename this to UpdateInChannels
        if (TRUE == ComponentGetUseInputsAtCouplingStepEndTime(comp)) {
            tmpTime = interval.startTime;
            interval.startTime = interval.endTime;

            retVal = DatabusTriggerConnectedInConnections(comp->GetDatabus(comp), &interval);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Update inports failed", comp->GetName(comp));
                return RETURN_ERROR;
            }

            interval.startTime = tmpTime;
        } else {
            retVal = DatabusTriggerConnectedInConnections(comp->GetDatabus(comp), &interval);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Update inports failed", comp->GetName(comp));
                return RETURN_ERROR;
            }
        }

#ifdef MCX_DEBUG
        if (time < MCX_DEBUG_LOG_TIME) {
            MCX_DEBUG_LOG("[%f] STORE IN: %s", comp->GetTime(comp), comp->GetName(comp));
        }
#endif // MCX_DEBUG

        if (TRUE == ComponentGetStoreInputsAtCouplingStepEndTime(comp)) {
            retVal = comp->Store(comp, CHANNEL_STORE_IN, interval.endTime, level);
        } else if (FALSE == ComponentGetStoreInputsAtCouplingStepEndTime(comp)) {
            retVal = comp->Store(comp, CHANNEL_STORE_IN, interval.startTime, level);
        } else {
            mcx_log(LOG_ERROR, "%s: storeInputsAtCouplingStepEndTime undefined", comp->GetName(comp));
            return RETURN_ERROR;
        }
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "%s: Storing inport failed", comp->GetName(comp));
            return RETURN_ERROR;
        }

#ifdef MCX_DEBUG
        if (time < MCX_DEBUG_LOG_TIME) {
            MCX_DEBUG_LOG("[%f] STEP (%s) [%f->%f]",
                          comp->GetTime(comp), comp->GetName(comp),
                          interval.startTime, interval.endTime);
        }
#endif // MCX_DEBUG

        retVal = ComponentDoStep(comp, group, interval.startTime, timeStep, interval.endTime, params->isNewStep);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "%s: DoStep failed", comp->GetName(comp));
            return RETURN_ERROR;
        }

        interval.startTime = comp->GetTime(comp);
        interval.endTime = comp->GetTime(comp);
        retVal = ComponentUpdateOutChannels(comp, &interval);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "%s: Updating outports failed", comp->GetName(comp));
            return RETURN_ERROR;
        }

        if (comp->PostDoStep) {
            retVal = comp->PostDoStep(comp);
            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "%s: PostDoStep failed", comp->GetName(comp));
                return RETURN_ERROR;
            }
        }

        /* the last coupling step is the new synchronization step */
        if (double_geq(comp->GetTime(comp), stepEndTime)) {
            level = STORE_SYNCHRONIZATION;
        } else {
            level = STORE_COUPLING;
        }

#ifdef MCX_DEBUG
        if (time < MCX_DEBUG_LOG_TIME) {
            MCX_DEBUG_LOG("[%f] STORE OUT: %s", comp->GetTime(comp), comp->GetName(comp));
        }
#endif // MCX_DEBUG

        retVal = comp->Store(comp, CHANNEL_STORE_OUT, comp->GetTime(comp), level);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "%s: Storing outport failed", comp->GetName(comp));
            return RETURN_ERROR;
        }
        retVal = comp->Store(comp, CHANNEL_STORE_LOCAL, comp->GetTime(comp), level);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "%s: Storing internal variables failed", comp->GetName(comp));
            return RETURN_ERROR;
        }
        retVal = comp->Store(comp, CHANNEL_STORE_RTFACTOR, comp->GetTime(comp), level);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "%s: Storing real time factors failed", comp->GetName(comp));
            return RETURN_ERROR;
        }
    }

    if (comp->GetFinishState(comp) == COMP_IS_FINISHED) {
        params->aComponentFinished = 1;
        mcx_log(LOG_WARNING, "%s: Element finished at time %f",
               comp->GetName(comp),
               comp->GetTime(comp));
    }

    return RETURN_OK;
}

static McxStatus CompTriggerInputs(CompAndGroup * compGroup, void * param) {
    const StepTypeParams * params = (const StepTypeParams *) param;
    Component * comp = compGroup->comp;

    TimeInterval interval = {comp->GetTime(comp), params->timeEndStep};

    McxStatus retVal = RETURN_OK;

    if (comp->HasOwnTime(comp)) {
        interval.startTime = comp->GetTime(comp);
        if (comp->GetTimeStep(comp) > 0.) {
            double timeStep = comp->GetTimeStep(comp);
            interval.endTime = interval.startTime + timeStep;
        } else {
            interval.endTime = params->timeEndStep;
        }
    }

    retVal = CompEnterCouplingStepMode(comp, param);
    if (RETURN_OK != retVal) {
        return RETURN_ERROR;
    }

    retVal = DatabusTriggerConnectedInConnections(comp->GetDatabus(comp), &interval);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "%s: Updating inports failed", comp->GetName(comp));
        return RETURN_ERROR;
    }

    MCX_DEBUG_LOG("[%f] STORE IN: %s", comp->GetTime(comp), comp->GetName(comp));

    if (TRUE == ComponentGetStoreInputsAtCouplingStepEndTime(comp)) {
        // No storing here as it has already happened in the last dostep
    } else if (FALSE == ComponentGetStoreInputsAtCouplingStepEndTime(comp)) {
        retVal = comp->Store(comp, CHANNEL_STORE_IN, interval.startTime, STORE_SYNCHRONIZATION);
    } else {
        mcx_log(LOG_ERROR, "%s: storeInputsAtCouplingStepEndTime undefined", comp->GetName(comp));
        return RETURN_ERROR;
    }
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "%s: Storing inport failed", comp->GetName(comp));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus CompDoStep(CompAndGroup * compGroup, void * param) {
    StepTypeParams * params = (StepTypeParams *) param;
    Component * comp = compGroup->comp;
    McxStatus retVal = RETURN_OK;

    retVal = ComponentDoCommunicationStep(compGroup->comp, compGroup->group, params);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "%s: DoStep failed", comp->GetName(comp));
        return RETURN_ERROR;
    }

    return retVal;
}

McxStatus CompEnterCouplingStepMode(Component * comp, void * param) {
    const StepTypeParams * params = (const StepTypeParams *) param;
    double timeStep = params->timeStepSize;
    McxStatus retVal = RETURN_OK;

    retVal = DatabusEnterCouplingStepMode(comp->GetDatabus(comp), timeStep);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "%s: Enter coupling step mode failed", comp->GetName(comp));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus CompCollectModeSwitchData(Component * comp, void * param) {
    McxStatus retVal = RETURN_OK;

    retVal = DatabusCollectModeSwitchData(comp->GetDatabus(comp));
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "%s: Collecting mode switch data failed", comp->GetName(comp));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}


McxStatus CompEnterCommunicationPoint(CompAndGroup * compGroup, void * param) {
    const StepTypeParams * params = (const StepTypeParams *) param;

    TimeInterval interval = {params->timeEndStep, params->timeEndStep};

    return ComponentEnterCommunicationPoint(compGroup->comp, &interval);
}


// ----------------------------------------------------------------------
// Step Type

static McxStatus StepTypeConfigure(StepType * stepType, StepTypeParams * params, SubModel * subModel) {
    return RETURN_OK;
}

static StepTypeType StepTypeGetType(StepType * stepType) {
    return stepType->type;
}

static McxStatus FinishComponent(Component * comp, void * data) {
    McxStatus retVal = RETURN_OK;
    FinishState * finishState = (FinishState *)data;
    if (comp->Finish) {
        mcx_signal_handler_set_name(comp->GetName(comp));
        retVal = comp->Finish(comp, finishState);
        mcx_signal_handler_unset_name();
    }
    return retVal;
}

McxStatus StepTypeFinish(StepType * stepType, StepTypeParams * params, SubModel * subModel, FinishState * finishState) {
    McxStatus retVal = RETURN_OK;

    retVal = subModel->LoopEvaluationList(subModel, CompTriggerInputs, (void *) params);
    if (RETURN_ERROR == retVal) {
        return retVal;
    }

    retVal = subModel->LoopComponents(subModel, FinishComponent, (void *) finishState);
    if (RETURN_ERROR == retVal) {
        return retVal;
    }

    return RETURN_OK;
}

static void StepTypeDestructor(void * self) {
}

static StepType * StepTypeCreate(StepType * type) {

    // Default functionality
    type->Configure = StepTypeConfigure;
    type->Finish = StepTypeFinish;
    type->GetType = StepTypeGetType;

    type->type = STEP_TYPE_UNDEFINED;

    return type;
}

OBJECT_CLASS(StepType, Object);


// ----------------------------------------------------------------------
// Step Type Params

static void StepTypeParamsDestructor(StepTypeParams * params) {

}

static StepTypeParams * StepTypeParamsCreate(StepTypeParams * params) {
    params->time = 0.;
    params->timeStepSize = 0.;
    params->timeEndStep = 0.;
    params->isNewStep = FALSE;
    params->numSteps = 0;
    params->aComponentFinished = FALSE;
    params->sumTime = FALSE;

    return params;
}

OBJECT_CLASS(StepTypeParams, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */