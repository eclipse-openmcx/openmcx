/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/Task.h"
#include "storage/ResultsStorage.h"
#include "steptypes/StepType.h"
#include "steptypes/StepTypeParallelST.h"
#include "steptypes/StepTypeParallelMT.h"
#include "steptypes/StepTypeSequential.h"
#include "core/Databus.h"
#include "core/channels/Channel.h"

#include "util/compare.h"
#include "util/signals.h"
#include "util/os.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static int TaskSubmodelIsFinished(SubModel * subModel) {
    size_t i = 0;
    ObjectContainer * eval = subModel->evaluationList;
    size_t numComps = eval->Size(eval);
    int modelContainsOnlyNeverFinishingComps = numComps ? TRUE : FALSE;

    for (i = 0; i < numComps; i++) {
        CompAndGroup * compAndGroup = (CompAndGroup *) eval->At(eval, i);
        Component * comp = (Component *) compAndGroup->comp;

        if (comp->GetFinishState(comp) == COMP_IS_NOT_FINISHED) {
            return FALSE;
        } else if (comp->GetFinishState(comp) != COMP_NEVER_FINISHES) {
            modelContainsOnlyNeverFinishingComps = FALSE;
        }
    }

    if (modelContainsOnlyNeverFinishingComps) {
        return FALSE;
    }

    return TRUE;
}

static int TaskCheckIfFinished(Task * task, SubModel * subModel, double time) {
    size_t i = 0;

    if (mcx_signal_handler_is_interrupted()) {
      return TRUE;
    }

    if (task->stopIfFirstComponentFinished) {
        if (task->params->aComponentFinished) {
            return TRUE;
        }
    }

    /* If all components are finished, finish regardless of
     * end time.
     */
    if (TaskSubmodelIsFinished(subModel)) {
        return TRUE;
    }

    if (task->timeEndDefined) {
        if (double_lt(time, task->timeEnd)) {
            return FALSE;
        } else {
            return TRUE;
        }
    }

    return FALSE;
}

static McxStatus TaskPrepareRun(Task * task, Model * model) {
    McxStatus retVal = RETURN_OK;

#if defined (ENABLE_STORAGE)
    retVal = task->storage->Setup(task->storage, task->timeStart);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Could not setup storage");
        return RETURN_ERROR;
    }

    retVal = task->storage->AddModelComponents(task->storage, model->subModel);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Could not setup component storage");
        return RETURN_ERROR;
    }

    retVal = task->storage->SetupBackends(task->storage);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Could not setup storage backends");
        return RETURN_ERROR;
    }
#endif //ENABLE_STORAGE

    task->finishState.stopIfFirstComponentFinished = task->stopIfFirstComponentFinished;
    return RETURN_OK;
}

static McxStatus TaskInitialize(Task * task, Model * model) {
    McxStatus retVal = RETURN_OK;

    SubModel * subModel = model->subModel;
    StepTypeParams * stepParams = task->params;

    retVal = model->Initialize(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Initialization of model failed");
        return RETURN_ERROR;
    }

    retVal = subModel->LoopComponents(subModel, CompPostDoUpdateState, (void *) task);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Post update state of elements failed during initialization");
        return RETURN_ERROR;
    }

    mcx_log(LOG_DEBUG, "Synchronization time-step-size: %g", stepParams->timeStepSize);

    task->storage->StoreModelOut(task->storage, model->subModel, stepParams->time, STORE_SYNCHRONIZATION);
    task->storage->StoreModelLocal(task->storage, model->subModel, stepParams->time, STORE_SYNCHRONIZATION);

    task->stepType->Configure(task->stepType, stepParams, subModel);

    return RETURN_OK;
}

static McxStatus TaskRun(Task * task, Model * model) {
    McxStatus retVal = RETURN_OK;
    McxStatus status = RETURN_OK;
    double preferredTimeStep = 0.;

    SubModel * subModel = model->subModel;
    StepTypeParams * stepParams = task->params;

    // for sumTime mode
    stepParams->timeEndStep = task->timeStart;

    while (!TaskCheckIfFinished(task, subModel, stepParams->time) && RETURN_ERROR != status) {
        /* for fixed time step sizes this is more accurate than summing all time steps */
        if (!stepParams->sumTime) {
            stepParams->timeEndStep = task->timeStart + (stepParams->numSteps + 1) * task->params->timeStepSize;
        } else {
            stepParams->timeEndStep += task->params->timeStepSize;
        }

        status = task->stepType->DoStep(task->stepType, stepParams, subModel);
        if (status != RETURN_OK) {
            break;
        }

        stepParams->numSteps++;
        stepParams->time = stepParams->timeEndStep;    // advance time

    }

    task->finishState.aComponentFinished = stepParams->aComponentFinished;
    task->finishState.errorOccurred = status == RETURN_ERROR;
    retVal = task->stepType->Finish(task->stepType, stepParams, subModel, &task->finishState);
    if (RETURN_ERROR == retVal) {
        status = RETURN_ERROR;
    }

    retVal = task->storage->Finished(task->storage);
    if (RETURN_ERROR == retVal) {
        status = RETURN_ERROR;
    }

    return status;
}

static void TaskDestructor(Task * task) {
    object_destroy(task->stepType);
    object_destroy(task->params);
    object_destroy(task->storage);
}

static McxStatus TaskRead(Task * task, TaskInput * taskInput) {
    McxStatus retVal;

    if (!task->config) {
        mcx_log(LOG_DEBUG, "Config is not set in simulation settings");
        return RETURN_ERROR;
    }

    mcx_log(LOG_INFO, "Reading settings:");

    // initial values
    task->params->isNewStep = 1;
    task->params->numSteps = 0;

    // process task information
    task->timeStart = taskInput->startTime.defined ? taskInput->startTime.value : 0.0;
    if (task->timeStart < 0.0) {
        mcx_log(LOG_ERROR, "Start time %g s cannot be smaller than 0.0 s", task->timeStart);
        return RETURN_ERROR;
    }
    mcx_log(LOG_INFO, "  Start time: %g s", task->timeStart);

    if (taskInput->endTime.defined) {
        task->timeEnd = taskInput->endTime.value;
        task->timeEndDefined = TRUE;
        mcx_log(LOG_INFO, "  End time: %g s", task->timeEnd);
    } else {
        task->timeEnd = 10.0;
        task->timeEndDefined = FALSE;
        mcx_log(LOG_INFO, "  End time: infinite");
    }

    if (taskInput->endType.defined && taskInput->endType.value == END_TYPE_FIRST_COMPONENT) {
        task->stopIfFirstComponentFinished = TRUE;
        mcx_log(LOG_INFO, "  Simulation stops if an element stops");
    }

    task->params->timeStepSize = taskInput->deltaTime.defined ? taskInput->deltaTime.value : 0.01;
    mcx_log(LOG_INFO, "  Synchronization time step: %g s", task->params->timeStepSize);

    task->params->sumTime = taskInput->sumTime.defined ? taskInput->sumTime.value : FALSE;
    if (task->config && task->config->sumTimeDefined) {
        task->params->sumTime = task->config->sumTime;
    }
    if (task->params->sumTime) {
        mcx_log(LOG_DEBUG, "  Using summation for time calculation");
    }

    task->stepTypeType = taskInput->stepType;
    switch(task->stepTypeType) {
    case STEP_TYPE_PARALLEL_ST:
        mcx_log(LOG_INFO, "  Type: Parallel (singlethreaded) Co-simulation");
        break;
    case STEP_TYPE_PARALLEL_MT:
        mcx_log(LOG_INFO, "  Type: Parallel (one time step) Co-simulation");
        break;
    case STEP_TYPE_SEQUENTIAL:
        mcx_log(LOG_INFO, "  Type: Sequential Co-simulation");
        break;
    default:
        /* this should not happen */
        mcx_log(LOG_ERROR, "Invalid coupling method");
        return RETURN_ERROR;
    }

    if (taskInput->inputAtEndTime.defined) {
        task->useInputsAtEndTime = taskInput->inputAtEndTime.value;
    } else {
        mcx_log(LOG_WARNING, "Input time not specified");
        task->useInputsAtEndTime = FALSE;
    }
    task->storeInputsAtEndTime = task->useInputsAtEndTime;

    if (task->useInputsAtEndTime) {
        mcx_log(LOG_DEBUG, "  Using input at end time as default");
    } else {
        mcx_log(LOG_DEBUG, "  Using input at start time as default");
    }

    mcx_log(LOG_INFO, " ");

    task->relativeEps = taskInput->relativeEps.defined ? taskInput->relativeEps.value : 1e-10;
    mcx_log(LOG_INFO, "  Epsilon: %g", task->relativeEps);
    mcx_log(LOG_INFO, " ");

    if (task->config->resultDir) { // the result directory is already given by command-line argument
        task->storage->resultPath = (char *) mcx_calloc(strlen(task->config->resultDir) + 1, sizeof(char));
        if (!task->storage->resultPath) {
            mcx_log(LOG_ERROR, "Memory allocation for name of result directory (from command line) failed");
            return RETURN_ERROR;
        }
        strcpy(task->storage->resultPath, task->config->resultDir);
        mcx_log(LOG_DEBUG, "Result directory (from command line): %s", task->storage->resultPath);
    }

    task->rtFactorEnabled = taskInput->timingOutput.defined ? taskInput->timingOutput.value : FALSE;
    retVal = task->storage->Read(task->storage, taskInput->results, task->config, IsStepTypeMultiThreading(task->stepTypeType));

    return retVal;
}

static McxStatus TaskSetup(Task * task, Model * model) {
    switch(task->stepTypeType) {
    case STEP_TYPE_PARALLEL_ST:
        task->stepType = (StepType *) object_create(StepTypeParallelST);
        break;
    case STEP_TYPE_SEQUENTIAL:
        task->stepType = (StepType *) object_create(StepTypeSequential);
        break;
#if defined (ENABLE_MT)
    case STEP_TYPE_PARALLEL_MT:
        task->stepType = (StepType *) object_create(StepTypeParallelMT);
        break;
#else //not ENABLE_MT
    case STEP_TYPE_PARALLEL_MT:
        task->stepType = (StepType *) object_create(StepTypeParallelST);
        break;
#endif //not ENABLE_MT
    default:
        mcx_log(LOG_ERROR, "Invalid coupling method");
        return RETURN_ERROR;
    }

    mcx_log(LOG_DEBUG, "Setting up simulation settings");
    mcx_log(LOG_DEBUG, " ");

    if (task->timeEndDefined) {
        if (task->timeEnd <= task->timeStart) {
            mcx_log(LOG_DEBUG, "The end time is smaller than the start time (%g s <= %g s)", task->timeEnd, task->timeStart);
            return RETURN_ERROR;
        }
    }

    task->params->time = task->timeStart;

    double_set_eps(task->relativeEps * task->params->timeStepSize);

    return RETURN_OK;
}

static StepTypeType TaskGetStepTypeType(Task * task) {
    return task->stepTypeType;
}

static void TaskSetConfig(Task * task, Config * config) {
    task->config = config;
}

static double TaskGetTimeStep(Task * task) {
    if (task->params) {
        return task->params->timeStepSize;
    } else {
        return 0.0;
    }
}

static Task * TaskCreate(Task * task) {
    task->params = (StepTypeParams *) object_create(StepTypeParams);
    if (!task->params) {
        mcx_log(LOG_DEBUG, "Could not create coupling method parameters");
        return NULL;
    }

    task->storage = (ResultsStorage *) object_create(ResultsStorage);
    if (!task->storage) {
        mcx_log(LOG_DEBUG, "Could not create storage");
        return NULL;
    }

    /* set function pointers */
    task->Read = TaskRead;
    task->Setup = TaskSetup;
    task->PrepareRun = TaskPrepareRun;
    task->Initialize = TaskInitialize;
    task->Run = TaskRun;

    task->SetConfig = TaskSetConfig;
    task->GetStepTypeType = TaskGetStepTypeType;

    task->GetTimeStep = TaskGetTimeStep;

    // set to default values
    task->timeStart = 0.;
    task->timeEnd = 0.;

    task->timeEndDefined = FALSE;
    task->stopIfFirstComponentFinished = FALSE;

    task->useInputsAtEndTime = FALSE;
    task->storeInputsAtEndTime = FALSE;

    task->relativeEps = 0.;

    task->rtFactorEnabled = FALSE;

    task->stepType = NULL;

    task->config = NULL;

    task->finishState.aComponentFinished = FALSE;
    task->finishState.stopIfFirstComponentFinished = FALSE;
    task->finishState.errorOccurred = FALSE;

    return task;
}

OBJECT_CLASS(Task, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */