/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_COMPONENT_H
#define MCX_CORE_COMPONENT_H

#include "CentralParts.h"
#include "components/ComponentFactory.h"
#include "core/Component_interface.h"
#include "core/Dependency.h"
#include "objects/StringContainer.h"
#include "objects/Vector.h"
#include "reader/model/components/ComponentInput.h"
#include "core/connections/ConnectionInfo.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum ComponentFinishState {
    COMP_IS_FINISHED,
    COMP_IS_NOT_FINISHED,
    COMP_NEVER_FINISHES
} ComponentFinishState;

struct Config;
struct Model;
struct ComponentData;
struct ChannelInfo;
struct Connection;
struct StepTypeParams;
struct ResultsStorage;
struct ComponentStorage;

typedef McxStatus (* fComponentRead)(Component * comp, ComponentInput * input, const struct Config * const config);
typedef McxStatus (* fComponentSetup)(Component * comp);
typedef McxStatus (* fComponentRegisterStorage)(Component * comp, struct ResultsStorage * storage);
typedef McxStatus (* fComponentInitialize)(Component * comp, size_t idx, double startTime);
typedef McxStatus (* fComponentExitInitializationMode)(Component * comp);
typedef McxStatus (* fComponentUpdateInitialOutChannels)(Component * comp);
typedef McxStatus (* fComponentUpdateOutChannels)(Component * comp);

typedef McxStatus (* fComponentUpdateInChannels)(Component * comp);

typedef int (* fComponentContainsComponent)(Component * comp, const Component * otherComp);

typedef struct ComponentStorage * (* fComponentGetStorage)(const Component * comp);
typedef McxStatus (* fComponentDoStep)(Component * comp, size_t group, double time,
    double deltaTime, double endTime, int isNewStep);
typedef McxStatus (*fComponentPostDoStep)(Component * comp);

typedef McxStatus (* fComponentFinish)(Component * comp, FinishState * finishState);
typedef size_t (* fComponentGetNumber)(const Component * comp);
typedef size_t (* fComponentGetOutGroup )(const Component * comp, size_t group);
typedef struct Dependencies * (* fComponentGetInOutDependency)(const Component * comp);

typedef McxStatus (* fComponentChannelRead)(Component * comp, struct ChannelInfo * info, PortInput * portInput, size_t idx);
typedef McxStatus (* fComponentChannelSetup)(Component * comp);

typedef McxStatus (* fComponentSetTimeStep)(Component * comp, double timeStep);
typedef void(* fComponentSetTime)(Component * comp, double time);
typedef int (* fComponentHasOwnTime)(const Component * comp);
typedef void(* fComponentSetHasOwnTime)(Component * comp);
typedef void (* fComponentUpdateTime)(Component * comp);

typedef int (* fComponentOneOutputOneGroup)(Component * comp);

typedef ComponentFinishState (* fComponentGetFinishState)(const Component * comp);
typedef void (* fComponentSetIsFinished)(Component * comp);

typedef McxStatus (* fOnConnectionsDone)(Component* comp);

typedef struct Databus * (* fComponentGetDatabus)(const Component * comp);
typedef const char * (* fComponentGetName)(const Component * comp);
typedef struct Model * (* fComponentGetModel)(const Component * comp);
typedef size_t (* fComponentGetID)(const Component * comp);
typedef int (* fComponentGetSequenceNumber)(const Component * comp);
typedef int (* fComponentGetCPUIdx)(const Component * comp);
typedef ObjectList * (*fComponentGetConnections)(Component * fromComp, Component * toComp);

typedef struct PpdLink * (* fGetPPDLink)(struct Component * comp);
typedef ChannelMode (* fGetChannelDefaultMode)(struct Component * comp);

typedef McxStatus (* fComponentPreDoUpdateState)(Component * comp, double time, double deltaTime);

typedef McxStatus (* fComponentPostDoUpdateState)(Component * comp, double time, double deltaTime);

typedef char * (* fComponentGetResultDir)(Component * comp);
typedef void (* fComponentSetModel)(Component * comp, struct Model * model);

typedef McxStatus (* fComponentStore)(Component * comp, ChannelStoreType chType, double time, StoreLevel level);

typedef int (* fComponentPredicate)(Component * comp);

typedef double (* fComponentGetDouble)(const Component * comp);
typedef McxStatus (*fComponentSetDouble)(Component * comp, double offset);

typedef void (*fComponentSetIsPartOfInitCalculation)(Component * comp, int isPartOfInitCalculation);


typedef McxStatus (* fAddObservableChannels)(const Component * comp, StringContainer * container, size_t * count);

extern const struct ObjectClass _Component;

struct Component {
    Object _;

    // Reads general component data from file
    fComponentRead Read;

    // Builds component according to data read by comp->Read
    fComponentSetup Setup;

    fComponentSetup SetupRTFactor;
    fComponentSetup SetupDatabus;

    fComponentInitialize Initialize;
    fComponentExitInitializationMode ExitInitializationMode;

    /**
     * Updates the initial values of the output channels depending on the
     * internal parameters and the input channels.
     */
    fComponentUpdateInitialOutChannels UpdateInitialOutChannels;

    fComponentUpdateOutChannels UpdateOutChannels;
    fComponentUpdateInChannels  UpdateInChannels;

    /**
     * Creates a new ComponentStorage, adds all its valid input and output channels,
     * and registers the ComponentStorage with the ResultStorage.
     */
    fComponentRegisterStorage RegisterStorage;

    fComponentGetStorage GetStorage;
    fComponentStore Store;

    fComponentDoStep DoStep;
    fComponentPostDoStep PostDoStep;
    fComponentFinish Finish;

    // Read and Setup Channels
    fComponentChannelRead      ChannelInRead;
    fComponentChannelSetup     ChannelInSetup;

    fComponentChannelRead      ChannelOutRead;
    fComponentChannelSetup     ChannelOutSetup;

    fGetChannelDefaultMode GetInChannelDefaultMode;

    // Data bus functions
    fComponentGetNumber GetNumInChannels;
    fComponentGetNumber GetNumOutChannels;
    fComponentGetNumber GetNumLocalChannels;
    fComponentGetNumber GetNumRTFactorChannels;

    fComponentGetNumber GetNumWriteInChannels;
    fComponentGetNumber GetNumWriteOutChannels;
    fComponentGetNumber GetNumWriteLocalChannels;
    fComponentGetNumber GetNumWriteRTFactorChannels;

    fComponentGetNumber GetNumConnectedOutChannels;

    fComponentGetNumber GetNumOutGroups;
    fComponentGetNumber GetNumInitialOutGroups;

    fComponentGetOutGroup GetOutGroup;
    fComponentGetOutGroup GetInitialOutGroup;

    fComponentGetInOutDependency GetInOutGroupsDependency;
    fComponentGetInOutDependency GetInOutGroupsInitialDependency;

    // For multi rate simulation
    fComponentGetDouble GetTime;
    fComponentSetTime SetTime;
    fComponentHasOwnTime HasOwnTime;

    fComponentSetHasOwnTime SetHasOwnTime;

    fComponentPredicate  IsPartOfInitCalculation;
    fComponentSetIsPartOfInitCalculation SetIsPartOfInitCalculation;

    fComponentGetFinishState GetFinishState;
    fComponentSetIsFinished SetIsFinished;

    fComponentGetDouble GetTimeStep;
    fComponentSetTimeStep SetTimeStep;
    fComponentSetTimeStep SetDefaultTimeStep;
    fComponentUpdateTime UpdateTime;

    fComponentSetModel SetModel;

    fComponentContainsComponent ContainsComponent;

    fComponentGetDatabus GetDatabus;
    fComponentGetName    GetName;
    fComponentGetName    GetType;
    fComponentGetModel   GetModel;
    fComponentGetID      GetID;

    fComponentOneOutputOneGroup OneOutputOneGroup;

    fComponentGetSequenceNumber GetSequenceNumber;
    fGetPPDLink GetPPDLink;

    fComponentGetConnections GetConnections;

    fComponentPreDoUpdateState PreDoUpdateState;

    fComponentPostDoUpdateState PostDoUpdateState;

    fComponentSetup WriteDebugInfoAfterSimulation; // TODO: find a better name for function pointer typedef

    fComponentGetResultDir GetResultDir;

    fComponentSetDouble SetResultTimeOffset;

    fComponentGetNumber GetNumObservableChannels;
    fAddObservableChannels AddObservableChannels;

    fOnConnectionsDone OnConnectionsDone;

    struct ComponentData * data;
};

/* these functions have to be called by subclasses */
void ComponentLog(const Component * comp, LogSeverity sev, const char * format, ...);

McxStatus ComponentRead(Component * comp, ComponentInput * input, const struct Config * const config);
McxStatus ComponentSetup(Component * comp);

McxStatus ComponentRegisterStorage(Component* comp, struct ResultsStorage* storage);

McxStatus ComponentInitialize(Component * comp, size_t group, double startTime);
McxStatus ComponentExitInitializationMode(Component * comp);
McxStatus ComponentBeforeDoSteps(Component * comp, void * param);

McxStatus ComponentUpdateOutChannels(Component * comp, TimeInterval * time);

McxStatus ComponentDoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep);

McxStatus ComponentDoCommunicationStep(Component * comp, size_t group, struct StepTypeParams * params);

McxStatus ComponentEnterCommunicationPoint(Component * comp, TimeInterval * time);
McxStatus ComponentEnterCommunicationPointForConnections(Component * comp, ObjectList * connections, TimeInterval * time);

Vector * GetInConnectionInfos(const Component * comp, size_t channelID);
struct ObjectContainer * GetInConnections(const Component * comp, size_t channelID);

size_t ComponentGetNumOutGroups(const Component * comp);
size_t ComponentGetNumInitialOutGroups(const Component * comp);

size_t ComponentGetOutGroup(const Component * comp, size_t idx);
size_t ComponentGetInitialOutGroup(const Component * comp, size_t idx);

struct Dependencies * ComponentGetInOutGroupsFullDependency(const Component * comp);

struct Dependencies * ComponentGetInOutGroupsNoDependency(const Component * comp);

McxStatus ComponentOutConnectionsEnterInitMode(Component * comp);
McxStatus ComponentDoOutConnectionsInitialization(Component * comp, int onlyIfDecoupled);
McxStatus ComponentOutConnectionsExitInitMode(Component * comp, double time);

double ComponentGetResultTimeOffset(struct Component * comp);

int ComponentGetHasOwnInputEvaluationTime(const Component * comp);
void ComponentSetHasOwnInputEvaluationTime(Component * comp, int flag);
int ComponentGetUseInputsAtCouplingStepEndTime(const Component * comp);
void ComponentSetUseInputsAtCouplingStepEndTime(Component * comp, int flag);
int ComponentGetStoreInputsAtCouplingStepEndTime(const Component * comp);
void ComponentSetStoreInputsAtCouplingStepEndTime(Component * comp, int flag);

Component * CreateComponentFromComponentInput(ComponentFactory * factory,
                                              ComponentInput * componentInput,
                                              const size_t id,
                                              const struct Config * const config);

Component * ComponentClone(ComponentFactory * factory,
                           const Component * source,
                           const char * cloneName,
                           const size_t id,
                           const struct Config * const config);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_COMPONENT_H */