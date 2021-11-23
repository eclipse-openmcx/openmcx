/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_COMPONENT_IMPL_H
#define MCX_CORE_COMPONENT_IMPL_H

#include "CentralParts.h"
#include "core/Component.h"
#include "util/time.h"

#include "reader/model/components/ComponentInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct Model;
struct Databus;

typedef struct ComponentRTFactorData ComponentRTFactorData;

struct ComponentRTFactorData {
    /* rtFactorDefined is set to true if the corresponding tag is
       written in the input file. The task->rtFactorEnabled flag is
       only used if rtFactorDefined == FALSE */
    int defined;
    int enabled;

    McxTime simClock; /* ticks in doStep since simulation start */
    McxTime stepClock; /* ticks in the current communication step */

    double simTime; /* time in doStep since simulation start */
    double simTimeTotal; /* time since initialize */
    double stepTime; /* time in the current communication step */

    double startTime; /* start time of simulation */
    double commTime; /* simulated time in current communication step */

    double rtFactor;
    double rtFactorAvg;

    McxTime startClock; /* wall clock of start of simulation */
    McxTime lastDoStepClock; /* wall clock of last DoStep */

    /* wall clock of last DoStep before entering communication mode */
    McxTime lastCommDoStepClock;

    double totalRtFactor;
    double totalRtFactorAvg;

    McxTime lastDoStepStartClock; /* wall clock of last DoStep Start*/
    McxTime lastDoStepEndClock; /* wall clock of last DoStep End */

    double calcStartWallClockTime;
    double calcEndWallClockTime;
};


typedef struct ComponentData ComponentData;

extern const struct ObjectClass _ComponentData;

struct ComponentData {
    Object _; // super class first

    double time;
    double timeStepSize;
    int hasOwnTime;
    long long numSteps;

    size_t countSnapTimeWarning;
    size_t maxNumTimeSnapWarnings;

    int sumTime;

    ComponentFinishState finishState;

    ComponentRTFactorData rtData;

    char * typeString;

    char * name;

    int triggerSequence;
    size_t id;

    int oneOutputOneGroup;
    int isPartOfInitCalculation;

    int hasOwnInputEvaluationTime; /*TRUE iff useInputsAtCouplingStepEndTime is set individually*/
    int useInputsAtCouplingStepEndTime;
    int storeInputsAtCouplingStepEndTime;

    struct Model * model;

    struct Databus * databus;

    struct ComponentStorage * storage;

    ComponentInput * input;
};

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_COMPONENT_IMPL_H */