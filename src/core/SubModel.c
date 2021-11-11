/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <limits.h>

#include "CentralParts.h"
#include "core/SubModel.h"
#include "core/Component.h"
#include "core/connections/Connection.h"
#include "core/channels/ChannelInfo.h"
#include "core/Databus.h"
#include "core/channels/Channel.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus SubModelGeneratorSetComponents(SubModelGenerator * subModelGenerator, ObjectContainer * comps,
                                                DependencyType depType);

static void CompAndGroupDestructor(CompAndGroup * compAndGroup) {
}

static CompAndGroup * CompAndGroupCreate(CompAndGroup * compAndGroup) {
    compAndGroup->comp = NULL;
    compAndGroup->group = 0;

    return compAndGroup;
}

OBJECT_CLASS(CompAndGroup, Object);

static McxStatus SubModelGeneratorSetComponentsList(SubModelGenerator * subModelGenerator, ObjectContainer * comps) {
    // Create components list
    if (subModelGenerator->componentsList) {
        object_destroy(subModelGenerator->componentsList);
    }
    subModelGenerator->componentsList = comps->Copy(comps);
    if (!subModelGenerator->componentsList) {
        mcx_log(LOG_ERROR, "Model: Could not copy submodel elements");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus SubModelGeneratorPrintNodeMap(SubModelGenerator * subModelGenerator,
                                               OrderedNodes * order,
                                               DependencyType depType) {
    size_t grp = 0; /* current group index */
    size_t n   = 0; /* current node index in group */

    ObjectContainer * nodes = subModelGenerator->nodeMap;

    if (!order) {
        mcx_log(LOG_ERROR, "Model: SubModelGeneratorPrintNodeMap: No ordered nodes given");
        return RETURN_ERROR;
    }

    for (grp = 0; grp < order->size; grp++) {
        for (n = 0; n < order->groups[grp]->nodes.size; n++) {
            CompAndGroup * compAndGroup = (CompAndGroup *) nodes->At(nodes, order->groups[grp]->nodes.values[n]);
            Component * comp = (Component *) compAndGroup->comp;
            Databus * db = comp->GetDatabus(comp);
            DatabusInfo * dbInfo = DatabusGetOutInfo(db);
            size_t numOutChannels = DatabusInfoGetChannelNum(dbInfo);
            char enu[12] = "       ";
            if (0 == n) {
                if (order->groups[grp]->isLoop) {
                    snprintf(enu, 12, "%2zu.loop", grp + 1 /* adjust for 0th elem */);
                } else {
                    snprintf(enu, 12, "%2zu.    ", grp + 1 /* adjust for 0th elem */);
                }
            }
            if (comp->OneOutputOneGroup(comp)) {
                ChannelInfo * info   = DatabusGetOutChannelInfo(comp->GetDatabus(comp), compAndGroup->group);
                mcx_log(LOG_INFO, " %s (%s, %s)", enu, comp->GetName(comp), ChannelInfoGetName(info));
                mcx_log(LOG_DEBUG, "         (%zu)", compAndGroup->group);
            } else if(depType == INITIAL_DEPENDENCIES && comp->GetNumInitialOutGroups(comp) == 1 ||
                      depType == RUNTIME_DEPENDENCIES && comp->GetNumOutGroups(comp) == 1) {
                mcx_log(LOG_INFO, " %s (%s, 0)", enu, comp->GetName(comp));
            } else if(comp->IsPartOfInitCalculation(comp) && (0 == numOutChannels) ) {
                mcx_log(LOG_INFO, " %s (%s, -)", enu, comp->GetName(comp));
            } else {
                ChannelInfo * info = DatabusGetOutChannelInfo(comp->GetDatabus(comp), compAndGroup->group);
                mcx_log(LOG_INFO, " %s (%s, %s)", enu, comp->GetName(comp), ChannelInfoGetName(info));
            }
        }
    }

    return RETURN_OK;
}

static McxStatus SubModelGeneratorFillNodeMap(SubModelGenerator * subModelGenerator, DependencyType depType) {
    size_t i, j, numOutGroups;

    ObjectContainer * comps = subModelGenerator->componentsList;
    ObjectContainer * nodes = subModelGenerator->nodeMap;

    McxStatus retVal = RETURN_OK;

    // Fill in all CompAndGroups
    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        if (depType == INITIAL_DEPENDENCIES) {
            numOutGroups = comp->GetNumInitialOutGroups(comp);
            if (comp->IsPartOfInitCalculation(comp) && (0 == numOutGroups) ) {
                numOutGroups = 1; //dummy outGroup for internal values evaluation of the comp (must depend on all input values!)
            }
        } else {
            numOutGroups = comp->GetNumOutGroups(comp);
        }

        for (j = 0; j < numOutGroups; j++) {
            CompAndGroup * compAndGroup = (CompAndGroup *) object_create(CompAndGroup);
            if (!compAndGroup) {
                mcx_log(LOG_ERROR, "Model: %s: Could not create node for group %d", comp->GetName(comp), j);
                return RETURN_ERROR;
            }

            compAndGroup->comp  = comp;
            compAndGroup->group = j;

            retVal = nodes->PushBack(nodes, (Object *) compAndGroup);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Model: %s: Could not add node for group %d", comp->GetName(comp), j);
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus SubModelGeneratorSetComponents(SubModelGenerator * subModelGenerator,
                                                ObjectContainer * comps,
                                                DependencyType depType) {
    McxStatus retVal = RETURN_OK;

    retVal = SubModelGeneratorSetComponentsList(subModelGenerator, comps);
    if (RETURN_OK != retVal) {
        return RETURN_ERROR;
    }

    retVal = SubModelGeneratorFillNodeMap(subModelGenerator, depType);
    if (RETURN_OK != retVal) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static size_t SubModelGeneratorGetNodeID(SubModelGenerator * subModelGenerator, Component * comp, size_t group) {
    ObjectContainer * nodes = subModelGenerator->nodeMap;

    size_t i;

    for (i = 0; i < nodes->Size(nodes); i++) {
        CompAndGroup * compAndGroup = (CompAndGroup *) nodes->At(nodes, i);

        if (compAndGroup->comp == comp && compAndGroup->group == group) {
            return i;
        }
    }

    return SIZE_T_ERROR;
}

static CompAndGroup * SubModelGeneratorGetCompAndGroup(SubModelGenerator * subModelGenerator, size_t nodeID) {
    ObjectContainer * nodes = subModelGenerator->nodeMap;

    return (CompAndGroup *) nodes->At(nodes, nodeID);
}

static ObjectContainer * GetEvaluationList(SubModelGenerator * subModelGenerator, OrderedNodes * order) {
    size_t i, j, elem = 0;
    McxStatus retVal;
    ObjectContainer * nodes = subModelGenerator->nodeMap;
    ObjectContainer * sortedList = (ObjectContainer *) object_create(ObjectContainer);

    for (i = 0; i < order->size; i++) {
        for (j = 0; j < order->groups[i]->nodes.size; j++) {
            if (elem < nodes->Size(nodes)) {
                size_t nodeID = order->groups[i]->nodes.values[j];
                retVal = sortedList->PushBack(sortedList, (Object *) SubModelGeneratorGetCompAndGroup(subModelGenerator, nodeID));
                elem++;
                if (RETURN_OK != retVal) {
                    object_destroy(sortedList);
                    return NULL;
                }
            }
        }
    }

    return sortedList;
}

ObjectContainer * GetEvaluationListOfAllNodes(SubModelGenerator * subModelGenerator) {
    size_t i = 0;
    ObjectContainer * nodes = subModelGenerator->nodeMap;
    ObjectContainer * sortedList = (ObjectContainer *) object_create(ObjectContainer);

    for (i = 0; i < nodes->Size(nodes); i++) {
        CompAndGroup * compAndGroup = SubModelGeneratorGetCompAndGroup(subModelGenerator, i);
        McxStatus retVal = sortedList->PushBack(sortedList, (Object *) compAndGroup);
        if (RETURN_OK != retVal) {
            object_destroy(sortedList);
            return NULL;
        }
    }

    return sortedList;
}

static ObjectContainer * GetEvaluationListOfGroup(SubModelGenerator * subModelGenerator, OrderedNodes * order, size_t group) {
    size_t j = 0;
    McxStatus retVal;
    ObjectContainer * sortedList = (ObjectContainer *) object_create(ObjectContainer);

    if (group < order->size) {
        for (j = 0; j < order->groups[group]->nodes.size; j++) {
            size_t nodeID = order->groups[group]->nodes.values[j];
            retVal = sortedList->PushBack(sortedList, (Object *) SubModelGeneratorGetCompAndGroup(subModelGenerator, nodeID));
            if (RETURN_OK != retVal) {
                object_destroy(sortedList);
                return NULL;
            }
        }
    } else {
        object_destroy(sortedList);
        return NULL;
    }

    return sortedList;
}

static ObjectContainer * GetComponentsOfEvaluationList(ObjectContainer * evaluationList) {
    size_t i = 0, j = 0;
    McxStatus retVal;
    ObjectContainer * comps = NULL;

    if (NULL == evaluationList) {
        return NULL;
    }

    comps = (ObjectContainer *) object_create(ObjectContainer);

    for (i = 0; i < evaluationList->Size(evaluationList); i++) {
        CompAndGroup * compAndGroup = (CompAndGroup *) evaluationList->At(evaluationList, i);
        Component * comp = (Component *) compAndGroup->comp;

        for (j = 0; j < comps->Size(comps); j++) {
            if (comp == (Component *) comps->At(comps, j)) {
                break;
            }
        }
        if (j == comps->Size(comps)) {
            retVal = comps->PushBack(comps, (Object *) comp);
            if (RETURN_OK != retVal) {
                object_destroy(comps);
                return NULL;
            }
        }
    }

    return comps;
}

static SubModel * SubModelGeneratorCreateSubModelOfAllNodes(SubModelGenerator * subModelGenerator, OrderedNodes * nodes) {
    ObjectContainer * comps = subModelGenerator->componentsList;
    Component * comp = NULL;
    size_t i;
    size_t numberOfElementsWithoutOutputsAndNotPartOfInitCalculation = 0;

    // No loops exist, so every group consists of exactly one component
    SubModel * subModel = NULL;
    for (i = 0; i < comps->Size(comps); i++) {
        comp = (Component *)comps->At(comps, i);
        Databus * db = comp->GetDatabus(comp);
        DatabusInfo * dbInfo = DatabusGetOutInfo(db);
        size_t numOutChannels = DatabusInfoGetChannelNum(dbInfo);
        if (!comp->IsPartOfInitCalculation(comp) && (0 == numOutChannels)) {
            numberOfElementsWithoutOutputsAndNotPartOfInitCalculation++;
        }
    }
    if (comps->Size(comps) - numberOfElementsWithoutOutputsAndNotPartOfInitCalculation > nodes->size) {
        mcx_log(LOG_ERROR, "Model: More components than available nodes");
        return NULL;
    }
    if (OrderedNodesCheckIfLoopsExist(nodes)) {
        mcx_log(LOG_ERROR, "Model: Cannot order submodel while loops exist");
        return NULL;
    }

    subModel = (SubModel *) object_create(SubModel);

    subModel->evaluationList = GetEvaluationList(subModelGenerator, nodes);
    subModel->components = GetComponentsOfEvaluationList(subModel->evaluationList);

    return subModel;
}

static SubModel * SubModelGeneratorCreateSubModelOfAllNodesAndBreakLoops(SubModelGenerator * subModelGenerator) {
    SubModel * subModel = (SubModel *) object_create(SubModel);

    subModel->evaluationList = GetEvaluationListOfAllNodes(subModelGenerator);
    subModel->components = subModelGenerator->componentsList->Copy(subModelGenerator->componentsList);

    return subModel;
}

static SubModel * SubModelGeneratorCreateSubModelOfGroup(SubModelGenerator * subModelGenerator, OrderedNodes * nodes, size_t group) {
    SubModel * subModel = (SubModel *) object_create(SubModel);

    subModel->evaluationList = GetEvaluationListOfGroup(subModelGenerator, nodes, group);
    if (subModel->evaluationList) {
        subModel->components = GetComponentsOfEvaluationList(subModel->evaluationList);
    } else {
        mcx_log(LOG_ERROR, "Model: CreateSubModel of group %d failed", group);
        object_destroy(subModel);
        return NULL;
    }

    return subModel;
}

static McxStatus LoopEvaluationList(ObjectContainer * evaluationList, fLoopEvaluationListFunction todo, const void * param) {
    CompAndGroup * evaluateElement = NULL;
    McxStatus retVal = RETURN_OK;
    size_t i   = 0;

    for (i = 0; i < evaluationList->Size(evaluationList); i++) {
        evaluateElement = (CompAndGroup *) evaluationList->At(evaluationList, i);

        if (evaluateElement->comp) {
            retVal = todo(evaluateElement, (void *) param);
            if (RETURN_ERROR == retVal) {
                return retVal;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus SubModelLoopEvaluationList(SubModel * subModel, fLoopEvaluationListFunction todo, void * param) {
    return LoopEvaluationList(subModel->evaluationList, todo, param);
}

static McxStatus LoopComponents(ObjectContainer * comps, fLoopComponentsListFunction todo, const void * param) {
    Component * comp = NULL;
    McxStatus retVal = RETURN_OK;
    size_t i   = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        comp = (Component *) comps->At(comps, i);

        if (comp) {
            retVal = todo(comp, (void *) param);
            if (RETURN_ERROR == retVal) {
                return retVal;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus SubModelLoopComponents(SubModel * subModel, fLoopComponentsListFunction todo, void * param) {
    return LoopComponents(subModel->components, todo, param);
}

static int SubModelIsElement(SubModel * subModel, const Component * comp) {
    ObjectContainer * comps = subModel->components;

    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * iComp = (Component *) comps->At(comps, i);
        if (NULL == iComp) {
            mcx_log(LOG_DEBUG, "Model: %s: nullptr in submodel", comp->GetName(comp));
        } else if (comp == iComp) {
            return 1;
        }
    }

    return 0;
}

static int SubModelContainsOrIsElement(SubModel * subModel, const Component * comp) {
    ObjectContainer * comps = subModel->components;
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * iComp = (Component *) comps->At(comps, i);
        if (!iComp) {
            mcx_log(LOG_DEBUG, "Model: nullptr in submodel at idx %zu", i);
            continue;
        }

        if (comp == iComp) {
            return TRUE;
        } else if (iComp->ContainsComponent && iComp->ContainsComponent(iComp, comp)) {
            return TRUE;
        }
    }

    return FALSE;
}

// only need to consider DECOUPLE_IFNEEDED since DECOUPLE_ALWAYS is handled at connection->Setup()
McxStatus OrderedNodesDecoupleConnections(OrderedNodes * orderedNodes, ObjectContainer * comps) {
    size_t i; size_t k;

    size_t numNodes = 0;
    size_t numGroups = orderedNodes->size;
    NodeGroup * group = NULL;

    for (i = 0; i < numGroups; i++) {
        group = orderedNodes->groups[i];
        numNodes += group->nodes.size;
    }

    if (comps->Size(comps) != numNodes) {
        mcx_log(LOG_ERROR, "Model: Calculated %d nodes, expected %d", numNodes, comps->Size(comps));
        return RETURN_ERROR;
    }

    // get edges to decouple
    for (i = 0; i < numGroups; i++) {
        group = orderedNodes->groups[i];
        if (group->isLoop) {
            size_t decoupleFrom = SIZE_T_ERROR, decoupleTo = SIZE_T_ERROR;
            int decouplePriority = -1;
            size_t ii = 0, jj = 0;

            for (ii = 0; ii < group->nodes.size && decouplePriority < INT_MAX; ii++) {
                for (jj = 0; jj < group->nodes.size && decouplePriority < INT_MAX; jj++) {
                    int localDecouplePriority = -1;
                    ObjectList * connections = NULL;
                    Component * fromComp = (Component *) comps->At(comps, group->nodes.values[ii]);
                    Component * toComp = (Component *) comps->At(comps, group->nodes.values[jj]);

                    // consistency check
                    if (fromComp->GetID(fromComp) != group->nodes.values[ii] || toComp->GetID(toComp) != group->nodes.values[jj]) {
                        mcx_log(LOG_ERROR, "Model: Elements need to be ordered by ID");
                        return RETURN_ERROR;
                    }

                    connections = fromComp->GetConnections(fromComp, toComp);
                    if (!connections) {
                        mcx_log(LOG_ERROR, "Model: Could not get connections from %s to %s", fromComp->GetName(fromComp), toComp->GetName(toComp));
                        return RETURN_ERROR;
                    } else if (0 == connections->Size(connections)) {
                        mcx_log(LOG_DEBUG, "no connection %s -> %s", fromComp->GetName(fromComp), toComp->GetName(toComp));
                    }

                    // check if all connections are decoupleable and get priority
                    for (k = 0; k < connections->Size(connections); k++) {
                        Connection * conn = (Connection *) connections->At(connections, k);
                        ConnectionInfo    * info = conn->GetInfo(conn);

                        if (ConnectionInfoIsDecoupled(info)) {
                            continue;
                        }

                        if (fromComp->GetSequenceNumber(fromComp) > toComp->GetSequenceNumber(toComp)) {
                            localDecouplePriority = INT_MAX; // ordering by components takes priority
                            break;
                        } else if (info->decoupleType == DECOUPLE_IFNEEDED) {
                            if (info->decouplePriority > localDecouplePriority) {
                                localDecouplePriority = info->decouplePriority;
                            }
                        } else if (info->decoupleType == DECOUPLE_NEVER) {
                            // if a connection in this bundle set to never decouple, discard this bundle
                            localDecouplePriority = -1;
                            break;
                        }
                    }

                    if (localDecouplePriority > decouplePriority) {
                        decoupleFrom = ii; decoupleTo = jj;
                        decouplePriority = localDecouplePriority;
                    } else {
                        // not highest priority
                    }

                    object_destroy(connections);
                }
            }

            // decouple connection with highest priority
            if (decoupleFrom != SIZE_T_ERROR && decoupleTo != SIZE_T_ERROR) {
                ObjectList * connections = NULL;
                Component * fromComp = (Component *) comps->At(comps, group->nodes.values[decoupleFrom]);
                Component * toComp = (Component *) comps->At(comps, group->nodes.values[decoupleTo]);

                // consistency check
                if (fromComp->GetID(fromComp) != group->nodes.values[decoupleFrom] || toComp->GetID(toComp) != group->nodes.values[decoupleTo]) {
                    mcx_log(LOG_ERROR, "Model: Elements need to be ordered by ID");
                    return RETURN_ERROR;
                }

                connections = fromComp->GetConnections(fromComp, toComp);
                if (!connections) {
                    mcx_log(LOG_ERROR, "Model: Could not get connections from %s to %s", fromComp->GetName(fromComp), toComp->GetName(toComp));
                    return RETURN_ERROR;
                }
                if (0 == connections->Size(connections)) {
                    mcx_log(LOG_ERROR, "Model: No connection from %s to %s", fromComp->GetName(fromComp), toComp->GetName(toComp));
                    return RETURN_ERROR;
                }

                // set all connections decoupled
                for (k = 0; k < connections->Size(connections); k++) {
                    Connection * conn = (Connection *) connections->At(connections, k);
                    ConnectionInfo * info = conn->GetInfo(conn);

                    char * connStr = ConnectionInfoConnectionString(info);
                    mcx_log(LOG_INFO, "Decoupling connection %s", connStr);
                    mcx_free(connStr);

                    ConnectionInfoSetDecoupled(info);
                }

                object_destroy(connections);
            } else {
                mcx_log(LOG_ERROR, "Model: No connection can be decoupled");
                // in this group, no bundle can be decoupled
                return RETURN_ERROR;
            }
        }
    }
    return RETURN_OK;
}

static void SubModelGeneratorDestructor(SubModelGenerator * subModelGenerator) {
    ObjectContainer * nodes = subModelGenerator->nodeMap;
    size_t i = 0;

    if (nodes) {
        for (i = 0; i < nodes->Size(nodes); i++) {
            Object * obj = nodes->At(nodes, i);
            object_destroy(obj);
        }
        object_destroy(subModelGenerator->nodeMap);
    }

    if (subModelGenerator->componentsList) {
        object_destroy(subModelGenerator->componentsList);
    }

    if (NULL != subModelGenerator->deps) {
        DependenciesDestroy(subModelGenerator->deps);
    }
}

static SubModelGenerator * SubModelGeneratorCreate(SubModelGenerator * subModel) {
    subModel->CreateSubModelOfAllNodes = SubModelGeneratorCreateSubModelOfAllNodes;

    subModel->CreateSubModelOfAllNodesAndBreakLoops = SubModelGeneratorCreateSubModelOfAllNodesAndBreakLoops;
    subModel->CreateSubModelOfGroup = SubModelGeneratorCreateSubModelOfGroup;

    subModel->SetComponents = SubModelGeneratorSetComponents;

    subModel->PrintNodeMap = SubModelGeneratorPrintNodeMap;

    subModel->nodeMap = (ObjectContainer *) object_create(ObjectContainer);
    if (!subModel->nodeMap) {
        return NULL;
    }
    subModel->componentsList = (ObjectContainer *) object_create(ObjectContainer);
    if (!subModel->componentsList) {
        return NULL;
    }

    subModel->deps = NULL;

    return subModel;
}

OBJECT_CLASS(SubModelGenerator, Object);

static void SubModelDestructor(SubModel * subModel) {
    object_destroy(subModel->evaluationList);
    object_destroy(subModel->components);

    object_destroy(subModel->outConnections);
}

static SubModel * SubModelCreate(SubModel * subModel) {
    subModel->LoopEvaluationList = SubModelLoopEvaluationList;
    subModel->LoopComponents = SubModelLoopComponents;
    subModel->IsElement = SubModelIsElement;
    subModel->ContainsOrIsElement = SubModelContainsOrIsElement;

    subModel->outConnections = (ObjectContainer *) object_create(ObjectContainer);
    if (!subModel->outConnections) {
        return NULL;
    }

    subModel->evaluationList = NULL;
    subModel->components = NULL;
    return subModel;
}

OBJECT_CLASS(SubModel, Object);

//------------------------------------------------------------------------------
// Component List

static size_t NumAllOutGroups(ObjectContainer * comps) {
    Component * comp = NULL;
    size_t i = 0;
    size_t num = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        comp = (Component *) comps->At(comps, i);
        num += comp->GetNumOutGroups(comp); // only used Out Channels
    }

    return num;
}

static size_t GetNodeID(ObjectContainer * comps, size_t compId) {
    /* return the place of the component with id compId in components */
    size_t i = 0;
    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        if (comp->GetID(comp) == compId)
            return i;
    }

    return SIZE_T_ERROR;
}

static struct Dependencies * SubModelGeneratorCreateDependencyMatrix(SubModelGenerator * subModelGenerator, DependencyType depType) {
    size_t numNodes = subModelGenerator->nodeMap->Size(subModelGenerator->nodeMap);
    McxStatus retVal = RETURN_OK;
    size_t targetNode;

    // init dependency matrix
    struct Dependencies * A = DependenciesCreate(numNodes, numNodes);
    if (!A) {
        return NULL;
    }

    for (targetNode = 0; targetNode < numNodes; targetNode++) {
        CompAndGroup * node = (CompAndGroup *) subModelGenerator->nodeMap->elements[targetNode];

        Component * targetComp  = node->comp;
        size_t         targetGroup = node->group;

        size_t numTargetInChannels = targetComp->GetNumInChannels(targetComp);
        size_t targetInChannelID = 0;

        struct Dependencies * targetCompDependency = NULL;

        if (INITIAL_DEPENDENCIES == depType) {
            if (targetComp->GetInOutGroupsInitialDependency) {
                targetCompDependency = targetComp->GetInOutGroupsInitialDependency(targetComp);
            }
        }

        if (NULL == targetCompDependency) {
            targetCompDependency = targetComp->GetInOutGroupsDependency(targetComp);
        }

        for (targetInChannelID = 0; targetInChannelID < numTargetInChannels; targetInChannelID++) {
            Dependency dependency = DEP_INDEPENDENT;
            retVal = GetDependency(targetCompDependency, targetInChannelID, targetGroup, &dependency);
            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "GetDependency failed in SubModelGeneratorCreateDependencyMatrix");
                mcx_free(A);
                return NULL;
            }

            if (INITIAL_DEPENDENCIES == depType) {
                Databus * db = targetComp->GetDatabus(targetComp);
                ChannelInfo * info =  DatabusGetInChannelInfo(db, targetInChannelID);
                // initial inputs are always exact
                if (info->initialValue) {
                    //check if connection exists (cosim init values are not deoupling connections, they only have lower priority than connection values)
                    ObjectContainer * infos = GetInConnectionInfos(targetComp, targetInChannelID);
                    size_t numInfos = infos->Size(infos);
                    size_t i = 0;

                    if (numInfos == 0) {        // no connections
                        dependency = DEP_INDEPENDENT;
                    } else {
                        int allDecoupled = TRUE;
                        for (i = 0; i < numInfos; i++) {
                            ConnectionInfo * info = (ConnectionInfo *) infos->At(infos, i);
                            if (!ConnectionInfoIsDecoupled(info)) {
                                allDecoupled = FALSE;
                                break;
                            }
                        }

                        if (allDecoupled) {     // decoupled connections
                            dependency = DEP_INDEPENDENT;
                        }
                    }
                }
            }

            if (DEP_INDEPENDENT != dependency) {
                ObjectContainer * infos = GetInConnectionInfos(targetComp, targetInChannelID);
                ObjectContainer * conns = GetInConnections(targetComp, targetInChannelID);
                size_t i = 0;

                for (i = 0; i < infos->Size(infos); i++) {
                    ConnectionInfo * info = infos->At(infos, i);
                    Connection * conn = conns->At(conns, i);

                    if (info && (info->decoupleType & (DECOUPLE_NEVER | DECOUPLE_IFNEEDED)) && (!ConnectionInfoIsDecoupled(info)) &&
                        conn && conn->IsActiveDependency(conn))
                    {
                        Component * sourceComp = info->sourceComponent;
                        size_t sourceOutGroup, sourceNode;
                        Databus * db = targetComp->GetDatabus(targetComp);
                        DatabusInfo * dbInfo = DatabusGetOutInfo(db);
                        size_t numOutChannels = DatabusInfoGetChannelNum(dbInfo);

                        if (INITIAL_DEPENDENCIES == depType) {
                            sourceOutGroup = sourceComp->GetInitialOutGroup(sourceComp, info->sourceChannel);
                        } else {
                            sourceOutGroup = sourceComp->GetOutGroup(sourceComp, info->sourceChannel);
                        }

                        sourceNode = SubModelGeneratorGetNodeID(subModelGenerator, sourceComp, sourceOutGroup);

                        if (SIZE_T_ERROR == sourceNode) {
                            // source is not part of this submodel
                            //   -> no dependency -> do nothing
                            continue;
                        }

                        if (INITIAL_DEPENDENCIES == depType) {
                            // check if the target output has an exact initial value
                            ChannelInfo * info = NULL;
                            // check if target outputs even exits
                            if (0 < numOutChannels) {
                                info = DatabusGetOutChannelInfo(db, targetGroup);
                                // initial outputs are exact only if specified
                                if (info->initialValueIsExact && info->initialValue) {
                                    continue;
                                }
                            }
                        }
                        retVal = SetDependency(A, sourceNode, targetNode, DEP_DEPENDENT);
                        if (RETURN_ERROR == retVal) {
                            mcx_log(LOG_ERROR, "SetDependency failed in SubModelGeneratorCreateDependencyMatrix");
                            mcx_free(A);
                            return NULL;
                        }

                        if (0 == numOutChannels && (INITIAL_DEPENDENCIES == depType)) {
                            mcx_log(LOG_DEBUG,
                                    "(%s,%d) -> (%s,-)",
                                    sourceComp->GetName(sourceComp),
                                    sourceOutGroup,
                                    targetComp->GetName(targetComp));
                        } else {
                            mcx_log(LOG_DEBUG,
                                    "(%s,%d) -> (%s,%d)",
                                    sourceComp->GetName(sourceComp),
                                    sourceOutGroup,
                                    targetComp->GetName(targetComp),
                                    targetGroup);
                        }
                    }
                }
            }
        }
        if (targetCompDependency) {
            DependenciesDestroy(targetCompDependency);
        }
    }

    return A;
}

OrderedNodes * CreateOrderedNodes(SubModelGenerator * subModelGenerator, DependencyType depType, int cutNodes) {
    ObjectContainer * nodes = subModelGenerator->nodeMap;
    OrderedNodes * orderedNodes = NULL;
    McxStatus retVal = RETURN_OK;
    int statusFlag = 0;
    struct Dependencies * A = NULL;

    size_t numAllNodes = nodes->Size(nodes);
    if (numAllNodes <= 0) {
        mcx_log(LOG_WARNING, "Model: No outports in elements");
    }

    A = SubModelGeneratorCreateDependencyMatrix(subModelGenerator, depType);
    if (!A) {
        mcx_log(LOG_ERROR, "Model: Could not object_create dependency matrix");
        return NULL;
    }

    if (subModelGenerator->deps) {
        DependenciesDestroy(subModelGenerator->deps);
    }

    subModelGenerator->deps = A;

    orderedNodes = tarjan((int *) GetDependencyMatrix(A), (int) numAllNodes);

    if (!orderedNodes) {
        mcx_log(LOG_ERROR, "Model: Tarjan Error");
        return NULL;
    }

    return orderedNodes;
}

int OrderedNodesCheckIfLoopsExist(OrderedNodes * nodes) {
    size_t i;
    size_t numGroups = nodes->size;
    NodeGroup * group = NULL;

    // here multithreading should be possible
    for (i = 0; i < numGroups; i++) {
        group = nodes->groups[i];
        if (group->isLoop) {
            return TRUE;
        }
    }

    return FALSE;
}

static size_t SubModelGetNumObservableChannels(const SubModel * subModel) {
    size_t count = 0;
    ObjectContainer * comps = subModel->components;
    Component * comp = NULL;
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        comp = (Component *) comps->At(comps, i);
        count += comp->GetNumObservableChannels(comp);
    }

    return count;
}

StringContainer * SubModelGetAllObservableChannelsContainer(SubModel * subModel) {
    ObjectContainer * comps = subModel->components;
    size_t numObservableChannels = SubModelGetNumObservableChannels(subModel);

    StringContainer * container = StringContainerCreate(numObservableChannels);
    size_t count = 0;
    size_t i = 0, j = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        // TODO: make composable
        comp->AddObservableChannels(comp, container, &count);
    }

    StringContainerResize(container, count);

    return container;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */