/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_STEPTYPES_STEP_TYPE_PARALLEL_MT_H
#define MCX_STEPTYPES_STEP_TYPE_PARALLEL_MT_H

#include "CentralParts.h"

#if defined (ENABLE_MT)

#include "steptypes/StepType.h"
#include "util/threads.h"
#include "util/events.h"
#include "util/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct DoStepThreadCounter DoStepThreadCounter;
typedef struct DoStepThreadArg DoStepThreadArg;

typedef void (* fDoStepThreadCounterSetup)(DoStepThreadCounter * counter, size_t count);
typedef void (* fDoStepThreadCounterDecrease)(DoStepThreadCounter * counter);
typedef void (* fDoStepThreadCounterWait)(DoStepThreadCounter * counter);

extern const struct ObjectClass _DoStepThreadCounter;

typedef struct DoStepThreadCounter {
    Object _;

    fDoStepThreadCounterSetup Setup;
    fDoStepThreadCounterDecrease Decrease;
    fDoStepThreadCounterWait Wait;

    McxMutex mutex;
    McxEvent isDoneEvent;

    // Is initialized to the number of components.
    // Whenever a component finishes its DoStep, count is decremented.
    // When count == 0, isDoneEvent is signaled.
    size_t count;
} DoStepThreadCounter;


typedef void (* fDoStepThreadArgSetup)(DoStepThreadArg * arg, Component * comp, size_t group, StepTypeParams * params);
typedef void (* fDoStepThreadArgSetCounter)(DoStepThreadArg * arg, DoStepThreadCounter * counter);
typedef void (* fDoStepThreadArgStartThread)(DoStepThreadArg * arg);

extern const struct ObjectClass _DoStepThreadArg;

typedef struct DoStepThreadArg {
    Object _;

    fDoStepThreadArgSetup Setup;
    fDoStepThreadArgSetCounter SetCounter;
    fDoStepThreadArgStartThread StartThread;

    // Data needed for ComponentDoCommStep
    struct Component * comp;
    size_t group;
    StepTypeParams * params;

    // last status value
    McxStatus status;

    // last reached synchronization point
    double reachedSyncTime;

    StepTypeSynchronization stepSizes;

    // flag if thread should stop
    int finished;

    // Event for when to start the DoStep calculation
    McxEvent startDoStepEvent;

    // Signal when last thread is done
    DoStepThreadCounter * threadCounter;
} DoStepThreadArg;


extern const struct ObjectClass _StepTypeParallelMT;

typedef struct StepTypeParallelMT {
    StepType _;

    DoStepThreadCounter * counter;
    ObjectContainer * args; /* of DoStepThreadArg */
    size_t numThreads;
    McxThread * threads;
} StepTypeParallelMT;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //ENABLE_MT

#endif /* MCX_STEPTYPES_STEP_TYPE_PARALLEL_MT_H */