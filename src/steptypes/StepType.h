/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_STEPTYPES_STEPTYPE_H
#define MCX_STEPTYPES_STEPTYPE_H

#include "CentralParts.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct StepType StepType;
typedef struct StepTypeParams StepTypeParams;
typedef struct Component Component;
typedef struct Model Model;
typedef struct SubModel SubModel;
typedef struct CompAndGroup CompAndGroup;


typedef struct StepTypeSynchronization {
    // flag whether the coupling and synchronization step size are multiples of one another
    int stepSizesAreMultiples;

    // epsilon used for time comparisons
    double eps;
} StepTypeSynchronization;

void StepTypeSynchronizationSetup(StepTypeSynchronization * stepTypes, Component * comp, double syncStepSize);
void StepTypeSynchronizationInit(StepTypeSynchronization * stepTypes);


typedef enum StepTypeType {
    STEP_TYPE_UNDEFINED = -1,       /* "parallel_singlethreaded" */
    STEP_TYPE_SEQUENTIAL = 1,       /* "sequential" */
    STEP_TYPE_PARALLEL_ST = 2,      /* "parallel_singlethreaded" */
    STEP_TYPE_PARALLEL_MT = 3,      /* "parallel_multithreaded" */
} StepTypeType;


int IsStepTypeMultiThreading(StepTypeType type);


typedef McxStatus (* fStepTypeDoStep)(StepType * stepType, StepTypeParams * params, SubModel * subModel);
typedef McxStatus (* fStepTypeFinish)(StepType * stepType, StepTypeParams * params, SubModel * subModel, FinishState * finishState);
typedef McxStatus (* fStepTypeConfigure)(StepType * stepType, StepTypeParams * params, SubModel * subModel, Model * model);
typedef StepTypeType (* fStepTypeGetType)(StepType * type);


extern const struct ObjectClass _StepTypeParams;

struct StepTypeParams {
    Object _; // super class first

    double time;
    double timeStepSize;
    double timeEndStep;
    int isNewStep;
    long long numSteps;
    int aComponentFinished;

    int sumTime; // if true then time = \sum_{numSteps} timeStepSize, else time = numSteps * timeStepSize
};

/* shared functionality between step types */
McxStatus ComponentDoCommunicationStep(Component * comp, size_t group, StepTypeParams * params);
McxStatus CompEnterCouplingStepMode(Component * comp, void * param);
McxStatus CompCollectModeSwitchData(Component * comp, void * param);
McxStatus CompEnterCommunicationPoint(CompAndGroup * compGroup, void * param);
McxStatus CompDoStep(CompAndGroup * compGroup, void * param);

McxStatus StepTypeFinish(StepType * stepType, StepTypeParams * params, SubModel * subModel, FinishState * finishState);



extern const struct ObjectClass _StepType;

struct StepType {
    Object _; // super class first

    StepTypeType type;

    fStepTypeDoStep DoStep;
    fStepTypeFinish Finish;
    fStepTypeConfigure Configure;
    fStepTypeGetType GetType;
};

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_STEPTYPES_STEPTYPE_H */