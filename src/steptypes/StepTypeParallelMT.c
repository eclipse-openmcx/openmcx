/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"

#if defined (ENABLE_MT)

#include "steptypes/StepTypeParallelMT.h"
#include "core/Component.h"
#include "core/SubModel.h"
#include "core/Databus.h"
#include "storage/ComponentStorage.h"
#include "storage/ResultsStorage.h"
#include "util/threads.h"
#include "util/events.h"
#include "util/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// DoStepThreadCounter

static void DoStepThreadCounterSetup(DoStepThreadCounter * counter, size_t count) {
    counter->count = count;
}

static void DoStepThreadCounterDecrease(DoStepThreadCounter * counter) {
    mcx_mutex_lock(&counter->mutex);
    if (counter->count > 0) {
        counter->count--;
        if (0 == counter->count) {
            mcx_event_set(&counter->isDoneEvent);
        }
    }
    mcx_mutex_unlock(&counter->mutex);
}

static void DoStepThreadCounterWait(DoStepThreadCounter * counter) {
    mcx_event_wait(&counter->isDoneEvent);
}

static void DoStepThreadCounterDestructor(DoStepThreadCounter * counter) {
    mcx_event_destroy(&counter->isDoneEvent);
    mcx_mutex_destroy(&counter->mutex);
}

static DoStepThreadCounter * DoStepThreadCounterCreate(DoStepThreadCounter * counter) {
    int status;

    counter->Setup = DoStepThreadCounterSetup;
    counter->Decrease = DoStepThreadCounterDecrease;
    counter->Wait = DoStepThreadCounterWait;

    mcx_mutex_create(&counter->mutex);
    counter->count = 0;
    status = mcx_event_create(&counter->isDoneEvent);
    if (status) {
        mcx_log(LOG_ERROR, "Simulation: Failed to create DoStep done event");
        return NULL;
    }
    return counter;
}

OBJECT_CLASS(DoStepThreadCounter, Object);

// ----------------------------------------------------------------------
// DoStepThreadArg

static void DoStepThreadArgSetup(DoStepThreadArg * arg, Component * comp, size_t group, StepTypeParams * params) {
    arg->comp = comp;
    arg->group = group;
    arg->params = params;

    StepTypeSynchronizationSetup(&arg->stepSizes, comp, params->timeStepSize);
}

static void DoStepThreadArgSetCounter(DoStepThreadArg * arg, DoStepThreadCounter * counter) {
    if (arg->threadCounter) {
        object_destroy(arg->threadCounter);
    }
    arg->threadCounter = (DoStepThreadCounter *) object_strong_reference(counter);
}

static void DoStepThreadArgStartThread(DoStepThreadArg * arg) {
    mcx_event_set(&arg->startDoStepEvent);
}

static void DoStepThreadArgDestructor(DoStepThreadArg * arg) {
    object_destroy(arg->threadCounter);
}

static DoStepThreadArg * DoStepThreadArgCreate(DoStepThreadArg * arg) {
    int status;

    arg->Setup = DoStepThreadArgSetup;
    arg->SetCounter = DoStepThreadArgSetCounter;
    arg->StartThread = DoStepThreadArgStartThread;

    arg->reachedSyncTime = 0.0;
    arg->comp = NULL;
    arg->group = 0;
    arg->params = NULL;
    arg->status = RETURN_OK;
    arg->finished = FALSE;

    StepTypeSynchronizationInit(&arg->stepSizes);

    status = mcx_event_create(&arg->startDoStepEvent);
    if (status) {
        mcx_log(LOG_ERROR, "Simulation: Failed to create DoStep start event");
        return NULL;
    }
    arg->threadCounter = NULL;

    return arg;
}

OBJECT_CLASS(DoStepThreadArg, Object);


static McxThreadReturn ParallelMTCompDoStepWrapper(void * arg) {
    DoStepThreadArg * threadArg = (DoStepThreadArg *) arg;
    StepTypeParams * params = (StepTypeParams *) threadArg->params;
    DoStepThreadCounter * counter = (DoStepThreadCounter *) threadArg->threadCounter;

    while (1) {
        mcx_event_wait(&threadArg->startDoStepEvent);

        if (threadArg->finished) {
            break;
        }

        threadArg->status = ComponentDoCommunicationStep(threadArg->comp, threadArg->group, params);
        if (RETURN_ERROR == threadArg->status) {
            mcx_log(LOG_ERROR, "Simulation: Element DoStep failed");
            // multithreaded error handling via threadArg->status
        }

        counter->Decrease(counter);
    }

    mcx_thread_exit(0);

    return 0;
}

static McxStatus ParallelMTDoStep(StepType * stepType, StepTypeParams * params, SubModel * subModel) {
    StepTypeParallelMT * parallelMTType = (StepTypeParallelMT *) stepType;

    DoStepThreadCounter * counter = parallelMTType->counter;

    ObjectContainer * args = parallelMTType->args;
    ObjectContainer * eval = subModel->evaluationList;

    McxStatus retVal = RETURN_OK;
    size_t i = 0;

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

    // right now, only this thread is running, so no mutex is needed
    counter->Setup(counter, eval->Size(eval));

    for (i = 0; i < eval->Size(eval); i++) {
        DoStepThreadArg * arg = (DoStepThreadArg *) args->At(args, i);
        arg->StartThread(arg);
    }
    // now all component->DoSteps are running

    counter->Wait(counter);

    /* check for errors during DoSteps */
    for (i = 0; i < eval->Size(eval); i++) {
        DoStepThreadArg * arg = (DoStepThreadArg *) args->At(args, i);
        if (RETURN_OK != arg->status) {
            mcx_log(LOG_ERROR, "Simulation: Synchronization DoStep from %fs to %fs failed", params->time, params->timeEndStep);
            return RETURN_ERROR;
        }
    }

    retVal = subModel->LoopEvaluationList(subModel, CompEnterCommunicationPoint, (void *) params);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Simulation: Enter communication point of elements failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ParallelMTConfigure(StepType * stepType, StepTypeParams * params, SubModel * subModel) {
    StepTypeParallelMT * parallelMTType = (StepTypeParallelMT *) stepType;

    DoStepThreadCounter * counter = parallelMTType->counter;

    ObjectContainer * args = parallelMTType->args;
    ObjectContainer * eval = subModel->evaluationList;

    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    parallelMTType->threads
        = (McxThread *) mcx_malloc(eval->Size(eval) * sizeof(McxThread));
    if (!parallelMTType->threads) {
        mcx_log(LOG_ERROR, "Simulation: Memory allocation for thread handles failed");
        return RETURN_ERROR;
    }

    // TODO: this should be done with subModel->LoopEvaluationList
    for (i = 0; i < eval->Size(eval); i++) {
        CompAndGroup * compAndGroup = (CompAndGroup *) eval->At(eval, i);
        DoStepThreadArg * arg = (DoStepThreadArg *) object_create(DoStepThreadArg);
        if (!arg) {
            mcx_log(LOG_ERROR, "Simulation: Could not create thread argument");
            return RETURN_ERROR;
        }

        arg->Setup(arg,
                   compAndGroup->comp,
                   compAndGroup->group,
                   params);

        arg->SetCounter(arg, counter);

        args->PushBack(args, (Object *) arg);
    }

    parallelMTType->numThreads = eval->Size(eval);
    for (i = 0; i < eval->Size(eval); i++) {
        DoStepThreadArg * arg = (DoStepThreadArg *) args->At(args, i);
        McxThread thread;
        int statusFlag = mcx_thread_create(&thread,
            (McxThreadStartRoutine) ParallelMTCompDoStepWrapper,
            arg);
        if (statusFlag) {
            mcx_log(LOG_ERROR, "Simulation: Could not create thread");
            return RETURN_ERROR;
        }
        parallelMTType->threads[i] = thread;
    }

    return RETURN_OK;
}


static void StepTypeParallelMTDestructor(StepTypeParallelMT * parallelMTType) {
    if (parallelMTType->args) {
        ObjectContainer * args = parallelMTType->args;
        size_t i = 0;

        for (i = 0; i < args->Size(args); i++) {
            DoStepThreadArg * arg = (DoStepThreadArg *) args->At(args, i);

            long ret;

            arg->finished = TRUE;
            arg->StartThread(arg);

            mcx_thread_join(parallelMTType->threads[i], &ret);

            object_destroy(arg);
        }

        object_destroy(parallelMTType->args);

        mcx_free(parallelMTType->threads);
        parallelMTType->threads = NULL;
    }

    if (parallelMTType->counter) {
        object_destroy(parallelMTType->counter);
    }
}


static StepTypeParallelMT * StepTypeParallelMTCreate(StepTypeParallelMT * parallelMTType) {
    StepType * type = (StepType *) parallelMTType;
    int statusFlag = 0;

    type->type = STEP_TYPE_PARALLEL_MT;

    type->Configure = ParallelMTConfigure;
    type->DoStep = ParallelMTDoStep;

    // counter
    parallelMTType->counter = (DoStepThreadCounter *) object_create(DoStepThreadCounter);
    if (!parallelMTType->counter) {
        mcx_log(LOG_ERROR, "Simulation: Could not create thread counter");
        return NULL;
    }

    // dynamic data
    parallelMTType->args = (ObjectContainer *) object_create(ObjectContainer);
    parallelMTType->threads = NULL;
    parallelMTType->numThreads = 0;

    return parallelMTType;
}

OBJECT_CLASS(StepTypeParallelMT, StepType);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //ENABLE_MT