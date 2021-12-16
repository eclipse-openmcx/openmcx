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

    McxTime rtCalcSum;        // ticks in doStep since simulation start
    double rtCalcSum_s;       // time in doSteps since simulation start

    McxTime rtCommStepTime;   // ticks in the current communication step
    double rtCommStepTime_s;  // time in the current communication step

    double simCommStepTime;   // simulated time in current communication step

    double rtTotalSum_s;      // time since initialize

    double simStartTime;      // start time of simulation

    double rtFactorCalc;
    double rtFactorCalcAvg;

    double rtFactorTotal;
    double rtFactorTotalAvg;

    McxTime rtCompStart;       // wall clock of start of component
    McxTime rtGlobalSimStart;  // wall clock of start of simulation

    McxTime rtLastEndCalc; // wall clock of last Calc End

    McxTime rtLastCompEnd; // wall clock of last DoStep before entering communication mode

    McxTime rtCalcStart; // wall clock of last DoStep Start
    double rtCalcStart_mys;

    McxTime rtCalcEnd;   // wall clock of last DoStep End
    double rtCalcEnd_mys;

    McxTime rtSyncStart;   // wall clock of sync start
    double rtSyncStart_mys;

    McxTime rtSyncEnd;   // wall clock of sync end
    double rtSyncEnd_mys;
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