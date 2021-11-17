/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_SUBMODEL_H
#define MCX_CORE_SUBMODEL_H

#include "objects/ObjectContainer.h"
#include "core/Component.h"
#include "tarjan.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef enum {
    INITIAL_DEPENDENCIES,
    RUNTIME_DEPENDENCIES
} DependencyType;

extern const struct ObjectClass _CompAndGroup;

typedef struct CompAndGroup {
    Object _; /* super class first */
    Component * comp;
    size_t group;
} CompAndGroup;

McxStatus CompPreDoUpdateState(Component * compGroup, void * param);

McxStatus CompPostDoUpdateState(Component * compGroup, void * param);

typedef struct SubModelGenerator SubModelGenerator;
typedef struct SubModel SubModel;

typedef McxStatus (* fSubModelGeneratorSetComponentsList)(SubModelGenerator * subModelGenerator,
                                                          ObjectContainer * components,
                                                          DependencyType depType);

typedef SubModel * (* fSubModelGeneratorCreateSubModel)(SubModelGenerator * subModelGenerator, OrderedNodes * nodes);
typedef SubModel * (* fSubModelGeneratorCreateSubModelAndBreakLoops)(SubModelGenerator * subModelGenerator);

typedef SubModel * (* fSubModelGeneratorCreateSubModelOfGroup)(SubModelGenerator * subModelGenerator, OrderedNodes * nodes, size_t group);

typedef McxStatus (* fSubModelGeneratorPrintNodeMap)(SubModelGenerator * subModelGenerator, OrderedNodes * order, DependencyType depType);

typedef int (* fSubModelIsElement)(SubModel * subModel, const Component * comp);

typedef McxStatus (* fLoopEvaluationListFunction)(CompAndGroup * item, void * param);
typedef McxStatus (* fSubModelLoopEvaluationList)(SubModel * subModel, fLoopEvaluationListFunction f, void * param);

typedef McxStatus (* fLoopComponentsListFunction)(Component * item, void * param);
typedef McxStatus (* fSubModelLoopComponents)(SubModel * subModel, fLoopComponentsListFunction f, void * param);

extern const struct ObjectClass _SubModel;

struct SubModel {
    Object _; // base class

    fSubModelLoopEvaluationList LoopEvaluationList;
    fSubModelLoopComponents LoopComponents;
    fSubModelIsElement IsElement;

    ObjectContainer * evaluationList; // contains CompAndGroup
    ObjectContainer * components; // contains Component

    ObjectContainer * outConnections;
};


StringContainer * SubModelGetAllObservableChannelsContainer(SubModel * subModel);

extern const struct ObjectClass _SubModelGenerator;

struct SubModelGenerator {
    Object _; // base class

    fSubModelGeneratorCreateSubModel CreateSubModelOfAllNodes;
    fSubModelGeneratorCreateSubModelAndBreakLoops CreateSubModelOfAllNodesAndBreakLoops;
    fSubModelGeneratorCreateSubModelOfGroup CreateSubModelOfGroup;

    fSubModelGeneratorSetComponentsList SetComponents;

    fSubModelGeneratorPrintNodeMap PrintNodeMap;
    // contains all CompAndGroup with their indices in the list
    // being their respective ids
    ObjectContainer * nodeMap;

    ObjectContainer * componentsList; // contains Component

    struct Dependencies * deps;
};

ObjectContainer * GetEvaluationListOfAllNodes(SubModelGenerator * subModelGenerator);

OrderedNodes * CreateOrderedNodes(SubModelGenerator * subModelGenerator, DependencyType depType, int cutNodes);
int OrderedNodesCheckIfLoopsExist(OrderedNodes * nodes);
McxStatus OrderedNodesDecoupleConnections(OrderedNodes * nodes, ObjectContainer * components);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_SUBMODEL_H */