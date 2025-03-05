/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_MODEL_H
#define MCX_CORE_MODEL_H

#include "CentralParts.h"

#include "core/Config.h"
#include "core/Task.h"
#include "core/Component.h"
#include "core/SubModel.h"
#include "reader/model/ModelInput.h"
#include "components/ComponentFactory.h"
#include "objects/Vector.h"

#ifdef __cplusplus
    extern "C" {
#endif /* __cplusplus */

struct ConnectionInfo;
struct Config;
struct Task;

struct Model;
typedef struct Model Model;

/* generic function definitions */

typedef McxStatus (* fModelSetup)(void * self);
typedef McxStatus (* fModelReadInput)(void * self, ModelInput * input);
typedef McxStatus (* fModelReadComponentsInput)(void * self, ComponentsInput * input);
typedef McxStatus (* fModelReadConnectionsInput)(void * self, ConnectionsInput * input);
typedef McxStatus (* fModelInitialize)(Model * model);

typedef void (* fModelSetTask)(Model * model, Task * task);
typedef void (* fModelSetConfig)(Model * model, Config * config);
typedef void (* fModelSetComponentFactory)(Model * model, ComponentFactory * factory);

typedef Task * (* fModelGetTask)(Model * model);

extern const struct ObjectClass _Model;

struct Model {
    Object _;

    fModelReadComponentsInput ReadComponents;
    fModelReadConnectionsInput ReadConnections;
    fModelReadInput Read;
    fModelSetup Setup;
    fModelInitialize Initialize;
    fModelSetConfig SetConfig;
    fModelSetTask   SetTask;
    fModelSetComponentFactory SetComponentFactory;
    fModelGetTask GetTask;

    Config * config;
    Task * task;

    Vector * connections;
    ObjectContainer * components;

    ComponentFactory * factory;

    SubModelGenerator * subModelGenerator;
    SubModelGenerator * initialSubModelGenerator;

    SubModel * subModel;
    SubModel * initialSubModel;             // submodel containing all nodes used for initialization

} ;

McxStatus ReadConnections(Vector * connections,
                          ConnectionsInput * connectionsInput,
                          ObjectContainer * components,
                          Component * sourceComp,
                          Component * targetComp);

McxStatus SetupComponents(ObjectContainer * components, Component * leaveOutComponent);
McxStatus SetupDatabusComponents(ObjectContainer * components);

McxStatus MakeConnections(Vector * connections, InterExtrapolatingType isInterExtrapolating);

McxStatus PrintModelGraph(ObjectContainer * comps, Vector * conns, Component * inComp, Component * outComp, const char * title, const char * filename);
McxStatus PrintComponentGraph(Component * comp, const char * filename,
                              struct Dependencies * A, DependencyType depType);

McxStatus ModelConnectionsEnterInitMode(ObjectContainer * comps);
McxStatus ModelConnectionsExitInitMode(ObjectContainer * comps, double time);
McxStatus ModelDoConnectionsInitialization(ObjectContainer * comps, int onlyIfDecoupled);

int ComponentsHaveVectorChannels(ObjectContainer * components);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_MODEL_H */