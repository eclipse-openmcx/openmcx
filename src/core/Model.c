/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/Model.h"

#include "core/Task.h"
#include "steptypes/StepType.h"
#include "tarjan.h"
#include "components/ComponentFactory.h"
#include "components/comp_constant.h"
#include "reader/model/ModelInput.h"
#include "reader/model/components/ComponentsInput.h"
#include "reader/model/components/ComponentInput.h"
#include "reader/model/connections/ConnectionsInput.h"

#include "core/connections/ConnectionInfoFactory.h"

#include "core/Databus.h"
#include "core/channels/Channel.h"
#include "core/channels/ChannelValueReference.h"
#include "core/channels/ChannelInfo.h"
#include "core/connections/Connection.h"
#include "core/connections/FilteredConnection.h"
#include "core/SubModel.h"

#include "storage/ComponentStorage.h"

#include "util/compare.h"
#include "util/string.h"
#include "util/signals.h"
#include "util/os.h"
#include "util/time.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static ChannelInfo * GetTargetChannelInfo(ConnectionInfo * info) {
    Component * trg = info->targetComponent;
    struct Databus * trgDb = trg->GetDatabus(trg);
    int trgId = info->targetChannel;

    return DatabusGetInChannelInfo(trgDb, trgId);
}

static ChannelInfo * GetSourceChannelInfo(ConnectionInfo * info) {
    Component * src = info->sourceComponent;
    struct Databus * srcDb = src->GetDatabus(src);
    int srcId = info->sourceChannel;

    return DatabusGetOutChannelInfo(srcDb, srcId);
}

static int ConnInfoWithSrc(void * elem, void * arg) {
    /*
     * TRUE if obj (ConnectionInfo) has ctx as its source ChannelInfo
     */
    ChannelInfo * chInfo = (ChannelInfo *) arg;
    ConnectionInfo * info = *((ConnectionInfo **) elem);
    ChannelInfo * srcInfo = GetSourceChannelInfo(info);

    return srcInfo == chInfo;
}

static int IsBinaryConn(void * elem, void * args) {
    /*
     * TRUE if both source and target channel infos of obj (ConnectionInfo) are binary
     */
    ConnectionInfo * info = (ConnectionInfo *) elem;
    ChannelInfo * srcInfo = GetSourceChannelInfo(info);
    ChannelInfo * trgInfo = GetTargetChannelInfo(info);

    return ChannelInfoIsBinary(srcInfo) && ChannelInfoIsBinary(trgInfo);
}

static int CanMakeChannelsBinReferences(Vector * connInfos, Task * task) {
    /*
     * Checks whether all binary connections in connInfos satisfy the condition
     * to use CHANNEL_BINARY_REFERENCE instead of CHANNEL_BINARY.
     *
     * If the time-steps of source and target of a binary connection
     * are equal, we do not have to copy the binary data and can use
     * CHANNEL_BINARY_REFERENCE instead.
     *
     * connInfos MUST contain binary connections with the same source channel.
     */
    size_t i = 0;
    size_t num = connInfos->Size(connInfos);

    for (i = 0; i < num; i++) {
        ConnectionInfo * info = *(ConnectionInfo **)connInfos->At(connInfos, i);

        Component * trg = info->targetComponent;
        Component * src = info->sourceComponent;

        ChannelInfo * trgInfo = GetTargetChannelInfo(info);
        ChannelInfo * srcInfo = GetSourceChannelInfo(info);

        // if a component (src, or trg) has its own time, it needs to agree with
        // the task time: comp->HasOwnTime() => comp->GetTimeStep() == task->GetTimeStep()
        if (!((!src->HasOwnTime(src) || src->GetTimeStep(src) == task->GetTimeStep(task)) &&
                (!trg->HasOwnTime(trg) || trg->GetTimeStep(trg) == task->GetTimeStep(task))
                ))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void UpdateBinaryChannelTypes(Vector * connInfos, Task * task) {
    /*
     * Updates the channel types of binary connections in connInfos according to the
     * result of `CanMakeChannelsBinReferences` (see function for more info)
     */
    size_t i = 0;

    int canMakeReference = CanMakeChannelsBinReferences(connInfos, task);

    for (i = 0; i < connInfos->Size(connInfos); i++) {
        ConnectionInfo * info = *(ConnectionInfo **)connInfos->At(connInfos, i);

        ChannelInfo * srcInfo = GetSourceChannelInfo(info);
        ChannelInfo * trgInfo = GetTargetChannelInfo(info);

        if (canMakeReference) {
            char * buffer = ConnectionInfoConnectionString(info);
            mcx_log(LOG_DEBUG, "Fast binary channel requirements fulfilled for connection %s", buffer);
            mcx_free(buffer);

            trgInfo->type = &ChannelTypeBinaryReference;
            srcInfo->type = &ChannelTypeBinaryReference;
        } else {
            char * buffer = ConnectionInfoConnectionString(info);
            mcx_log(LOG_DEBUG, "Using binary channels for connection %s", buffer);
            mcx_free(buffer);

            trgInfo->type = &ChannelTypeBinary;
            srcInfo->type = &ChannelTypeBinary;
        }
    }
}

/**
 * Remove connections to Constant elements from the model
 * For each such connection, the value of the Constant shall be taken
 * and set as the default value of the corresponding input.
 * Necessary value/type conversions are done as well.
 */
static McxStatus ModelPreprocessConstConnections(Model * model) {
    Vector * conns = model->connections;
    Vector * filteredConns = NULL;

    size_t i = 0;
    McxStatus retVal = RETURN_OK;

    filteredConns = (Vector*)object_create(Vector);
    if (!filteredConns) {
        mcx_log(LOG_ERROR, "Not enough memory to filter out constant connections");
        return RETURN_ERROR;
    }
    filteredConns->Setup(filteredConns, sizeof(ConnectionInfo), ConnectionInfoInit, NULL, NULL);

    for (i = 0; i < conns->Size(conns); i++) {
        ConnectionInfo * info = (ConnectionInfo *) conns->At(conns, i);
        ChannelValue * src = NULL;

        Component * srcComp = info->sourceComponent;
        CompConstant * srcCompConst = NULL;
        Databus * srcDb = srcComp->GetDatabus(srcComp);
        ChannelInfo * srcChannelInfo = DatabusGetOutChannelInfo(srcDb, info->sourceChannel);
        ChannelDimension * srcDim = NULL;

        Component * trgComp = info->targetComponent;
        Databus * trgDb = trgComp->GetDatabus(trgComp);
        ChannelInfo * trgChannelInfo = DatabusGetInChannelInfo(trgDb, info->targetChannel);
        ChannelDimension * trgDim = NULL;

        // if not a const conn, add to the filtered conns
        if (0 != strcmp(compConstantTypeString, srcComp->GetType(srcComp))) {
            filteredConns->PushBack(filteredConns, info);
            continue;
        }

        // else kick the connection and update the channel default value
        srcCompConst = (CompConstant *) srcComp;
        src = (ChannelValue *) mcx_calloc(1, sizeof(ChannelValue));

        if (!src) {
            mcx_log(LOG_ERROR, "ModelPreprocessConstConnections: Not enough memory");
            goto cleanup_1;
        }

        ChannelValueInit(src, ChannelTypeClone(srcChannelInfo->type));
        retVal = ChannelValueSet(src, srcCompConst->GetValue(srcCompConst, info->sourceChannel));
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        if (!trgChannelInfo->defaultValue) {
            trgChannelInfo->defaultValue = (ChannelValue *) mcx_calloc(1, sizeof(ChannelValue));
            ChannelValueInit(trgChannelInfo->defaultValue, ChannelTypeClone(trgChannelInfo->type));
        }

        // prepare slice dimensions
        srcDim = CloneChannelDimension(info->sourceDimension);
        if (info->sourceDimension && !srcDim) {
            mcx_log(LOG_ERROR, "ModelPreprocessConstConnections: Source dimension slice allocation failed");
            goto cleanup_1;
        }

        retVal = ChannelDimensionAlignIndicesWithZero(srcDim, srcChannelInfo->dimension);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "ModelPreprocessConstConnections: Source dimension normalization failed");
            goto cleanup_1;
        }

        trgDim = CloneChannelDimension(info->targetDimension);
        if (info->targetDimension && !trgDim) {
            mcx_log(LOG_ERROR, "ModelPreprocessConstConnections: Target dimension slice allocation failed");
            goto cleanup_1;
        }

        retVal = ChannelDimensionAlignIndicesWithZero(trgDim, trgChannelInfo->dimension);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "ModelPreprocessConstConnections: Target dimension normalization failed");
            goto cleanup_1;
        }

        // out channel range and linear conversions
        retVal = ConvertRange(srcChannelInfo->min, srcChannelInfo->max, src, srcDim);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        retVal = ConvertLinear(srcChannelInfo->scale, srcChannelInfo->offset, src, srcDim);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        // type conversion
        retVal = ConvertType(trgChannelInfo->defaultValue, trgDim, src, srcDim);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        // unit conversion
        retVal = ConvertUnit(srcChannelInfo->unitString, trgChannelInfo->unitString, trgChannelInfo->defaultValue, trgDim);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        object_destroy(srcDim);
        object_destroy(trgDim);

        continue;

cleanup_1:
        if (src) {
            ChannelValueDestructor(src);
            mcx_free(src);
        }

        object_destroy(srcDim);
        object_destroy(trgDim);

        retVal = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(filteredConns);

        return RETURN_ERROR;
    }

    object_destroy(conns);
    model->connections = filteredConns;

    return RETURN_OK;
}

static int ConnInfoContained(void * elem, void * arg) {
    ConnectionInfo * info = *(ConnectionInfo **) elem;
    return info == (ConnectionInfo *) arg;
}

static McxStatus ModelPreprocessBinaryConnections(Model * model) {
    Vector * conns = model->connections;
    size_t connsSize = conns->Size(conns);

    Vector * binConnInfos = NULL;
    Vector * processedConnInfos = NULL;

    size_t i = 0;

    Task * task = model->task;

    McxStatus retVal = RETURN_OK;

    if (task->stepTypeType != STEP_TYPE_SEQUENTIAL) {
        return RETURN_OK;
    }

    binConnInfos = conns->FilterRef(conns, IsBinaryConn, NULL);
    if (!binConnInfos) {
        mcx_log(LOG_ERROR, "Not enough memory for binary connections");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    processedConnInfos = (Vector*)object_create(Vector);
    if (!processedConnInfos) {
        mcx_log(LOG_ERROR, "Not enough memory for processed binary connections");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    processedConnInfos->Setup(processedConnInfos, sizeof(ConnectionInfo*), NULL, NULL, NULL);

    for (i = 0; i < binConnInfos->Size(binConnInfos); i++) {
        ConnectionInfo * info = *(ConnectionInfo **)binConnInfos->At(binConnInfos, i);
        ChannelInfo * srcInfo = GetSourceChannelInfo(info);
        Vector* connInfos = NULL;

        if (processedConnInfos->FindIdx(processedConnInfos, ConnInfoContained, info) != SIZE_T_ERROR) {
            continue;
        }

        connInfos = binConnInfos->Filter(binConnInfos, ConnInfoWithSrc, srcInfo);
        if (!connInfos) {
            mcx_log(LOG_ERROR, "Not enough memory for filtered binary connections");
            retVal = RETURN_ERROR;
            goto cleanup;
        }

        UpdateBinaryChannelTypes(connInfos, task);

        retVal = processedConnInfos->Append(processedConnInfos, connInfos);
        object_destroy(connInfos);

        if (retVal == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "Appending processed binary connections failed");
            goto cleanup;
        }
    }

cleanup:
    object_destroy(binConnInfos);
    object_destroy(processedConnInfos);

    return retVal;
}

static int ComponentIsBoundaryCondition(const Component * comp) {
    if (0 == strcmp(compConstantTypeString, comp->GetType(comp))
        )
    {
        return TRUE;
    }
    return FALSE;
}

static Vector * GetAllSourceConnInfoRefs(Model * model, Component * comp) {
    Vector * conns = model->connections;
    size_t numConnections = conns->Size(conns);
    Vector * sources = (Vector*) object_create(Vector);
    size_t i = 0;

    sources->Setup(sources, sizeof(ConnectionInfo*), NULL, NULL, NULL);
    for (i = 0; i < numConnections; i++) {
        ConnectionInfo * info = (ConnectionInfo *) conns->At(conns, i);
        Component * target = info->targetComponent;
        if (target == comp) {
            sources->PushBack(sources, &info);
        }
    }

    return sources;
}

static ObjectContainer * GetAllSourceElements(Model * model, Component * comp) {
    Vector * conns = model->connections;
    size_t numConnections = conns->Size(conns);
    ObjectContainer * sources = (ObjectContainer *) object_create(ObjectContainer);
    size_t i = 0;

    for (i = 0; i < numConnections; i++) {
        ConnectionInfo * info = (ConnectionInfo *) conns->At(conns, i);
        Component * target = info->targetComponent;
        if (target == comp) {
            Component * source = info->sourceComponent;
            if (!sources->Contains(sources, (Object *) source)) {
                sources->PushBack(sources, (Object *) source);
            }
        }
    }

    return sources;
}

static McxStatus ModelPreprocess(Model * model) {
    McxStatus retVal = RETURN_OK;

    retVal = ModelPreprocessConstConnections(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Preprocessing const connections failed");
        return RETURN_ERROR;
    }

    retVal = ModelPreprocessBinaryConnections(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Preprocessing binary connections failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ModelInsertAllFilters(Model * model) {
    ObjectContainer * comps = model->components;
    size_t i = 0;
    size_t j = 0;
    size_t k = 0;

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        struct Databus * db = comp->GetDatabus(comp);
        size_t numOutChannels = DatabusGetOutChannelsNum(db);

        for (j = 0; j < numOutChannels; j++) {
            ChannelOut * out = DatabusGetOutChannel(db, j);
            ObjectList * conns = out->GetConnections(out);

            for (k = 0; k < conns->Size(conns); k++) {
                Connection * connection = (Connection *) conns->At(conns, k);
                ConnectionInfo * info = connection->GetInfo(connection);
                    if (connection->AddFilter) {
                        char * connStr = ConnectionInfoConnectionString(info);
                        mcx_log(LOG_DEBUG, "  Adding filter to connection: %s", connStr);
                        if (connStr) {
                            mcx_free(connStr);
                        }
                        if (connection->AddFilter(connection) != RETURN_OK) {
                            retVal = RETURN_ERROR;
                        }
                    }
            }
        }
    }

    return retVal;
}

static McxStatus ModelCreateInitSubModel(Model * model) {
    OrderedNodes * orderedNodes = NULL;

    McxStatus retVal = RETURN_OK;

    model->initialSubModelGenerator->SetComponents(
        model->initialSubModelGenerator,
        model->components,
        INITIAL_DEPENDENCIES
    );

    orderedNodes = CreateOrderedNodes(model->initialSubModelGenerator, INITIAL_DEPENDENCIES, FALSE);
    if (!orderedNodes) {
        mcx_log(LOG_ERROR, "Model: Could not create initialization evaluation list");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_log(LOG_INFO, "Initialization evaluation order from model input file (element, group):");
    retVal = model->initialSubModelGenerator->PrintNodeMap(model->initialSubModelGenerator, orderedNodes, INITIAL_DEPENDENCIES);
    if (retVal != RETURN_OK) {
        goto cleanup;
    }

    if (OrderedNodesCheckIfLoopsExist(orderedNodes)) {
        mcx_log(LOG_ERROR, "Model: Found loops in the initialization topology");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    model->initialSubModel = model->initialSubModelGenerator->CreateSubModelOfAllNodes(model->initialSubModelGenerator,
                                                                                       orderedNodes);
    if (model->initialSubModel == NULL) {
        mcx_log(LOG_ERROR, "Model: Could not create initialization submodel");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (orderedNodes) {
        tarjan_ordered_nodes_cleanup(orderedNodes);
    }

    return retVal;
}

McxStatus ModelInitInConnectedList(ObjectContainer * comps) {
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *)comps->At(comps, i);
        McxStatus retVal = DatabusUpdateInConnected(comp->GetDatabus(comp));
        if (RETURN_ERROR == retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not initialize the In-Connection list");
            return RETURN_ERROR;
        }
    }
    return RETURN_OK;
}

static McxStatus ModelSignalConnectionsDone(ObjectContainer * comps) {
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *)comps->At(comps, i);
        if (NULL != comp->OnConnectionsDone) {
            if (RETURN_ERROR == comp->OnConnectionsDone(comp)) {
                ComponentLog(comp, LOG_ERROR, "OnConnectionsDone callback failed");
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

int ComponentsHaveVectorChannels(ObjectContainer * components) {
    size_t numComps = components->Size(components);
    size_t i = 0;

    for (i = 0; i < numComps; i++) {
        Component * comp = (Component *) components->At(components, i);
        Databus * db = comp->GetDatabus(comp);
        size_t numIns = DatabusGetInChannelsNum(db);
        size_t numOuts = DatabusGetOutChannelsNum(db);
        size_t j = 0;

        for (j = 0; j < numIns; j++) {
            ChannelInfo * info = DatabusGetInChannelInfo(db, j);
            if (ChannelTypeIsArray(info->type)) {
                return TRUE;
            }
        }

        for (j = 0; j < numOuts; j++) {
            ChannelInfo* info = DatabusGetOutChannelInfo(db, j);
            if (ChannelTypeIsArray(info->type)) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static McxStatus ModelConnectionsDone(Model * model) {
    OrderedNodes * orderedNodes = NULL;
    McxStatus retVal = RETURN_OK;
    int i = 0;
    char * tarjanString = NULL;

    if (model->subModel) {
        goto cleanup;
    }

    retVal = ModelInitInConnectedList(model->components);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: In-Connection lists could not be initialized");
        goto cleanup;
    }

    retVal = ModelSignalConnectionsDone(model->components);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Components could not be notified about connection completion");
        goto cleanup;
    }

    // determine initialization evaluation order
    if (model->config->cosimInitEnabled) {
        // separate triggering of array elements is not supported at the moment
        // therefore some models with array ports might not work
        if (ComponentsHaveVectorChannels(model->components)) {
            mcx_log(LOG_ERROR, "Co-Simulation initialization: Components with vector ports are not supported");
            return RETURN_ERROR;
        }

        retVal = ModelCreateInitSubModel(model);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Model: InitSubModel could not be created");
            goto cleanup;
        }
    }

    model->subModelGenerator->SetComponents(
        model->subModelGenerator,
        model->components,
        RUNTIME_DEPENDENCIES);

    // Strict Model
    orderedNodes = CreateOrderedNodes(model->subModelGenerator, RUNTIME_DEPENDENCIES, FALSE);
    if (!orderedNodes) {
        mcx_log(LOG_ERROR, "Model: Could not create runtime evaluation list");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_log(LOG_INFO, "Runtime evaluation order from model input file (element, group):");
    retVal = model->subModelGenerator->PrintNodeMap(model->subModelGenerator, orderedNodes, RUNTIME_DEPENDENCIES);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Could not print node map");
        goto cleanup;
    }

    while (OrderedNodesCheckIfLoopsExist(orderedNodes)) {
        i += 1; /* num iterations */

        retVal = OrderedNodesDecoupleConnections(orderedNodes, model->components);
        if (retVal != RETURN_OK) {
            mcx_log(LOG_ERROR, "Model: Unable to decouple algebraic loop in iteration %d", i);
            goto cleanup;
        }

        if (orderedNodes) {
            tarjan_ordered_nodes_cleanup(orderedNodes);
        }

        orderedNodes = CreateOrderedNodes(model->subModelGenerator, RUNTIME_DEPENDENCIES, FALSE);
        if (!orderedNodes) {
            mcx_log(LOG_ERROR, "Model: Could not create evaluation list in iteration %d", i);
            retVal = RETURN_ERROR;
            goto cleanup;
        }
#ifdef MCX_DEBUG
        // print intermediate step during decoupling
        retVal = model->subModelGenerator->PrintNodeMap(model->subModelGenerator, orderedNodes, RUNTIME_DEPENDENCIES);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Model: Could not print node map in iteration %d", i);
            retVal = RETURN_ERROR;
            goto cleanup;
        }
#endif // MCX_DEBUG
    }

    if (i > 0) {
        mcx_log(LOG_INFO, "Runtime evaluation order of decoupled model (element, group):");
        retVal = model->subModelGenerator->PrintNodeMap(model->subModelGenerator, orderedNodes, RUNTIME_DEPENDENCIES);
    }

    model->subModel = model->subModelGenerator->CreateSubModelOfAllNodes(model->subModelGenerator, orderedNodes);
    if (NULL == model->subModel) {
        mcx_log(LOG_ERROR, "Model: Could not create runtime submodel");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    retVal = model->subModel->LoopComponents(model->subModel, CompCollectModeSwitchData, NULL);
    if (RETURN_ERROR == retVal) {
        return retVal;
    }

cleanup:
    if (orderedNodes) {
        tarjan_ordered_nodes_cleanup(orderedNodes);
    }

    return retVal;
}

static Connection * ModelGetConnectionFromInfo(Model * model, ConnectionInfo * info) {
    ObjectContainer * comps = model->components;
    size_t i = 0;
    size_t j = 0;
    size_t k = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        struct Databus * db = comp->GetDatabus(comp);
        size_t numOutChannels = DatabusGetOutChannelsNum(db);

        for (j = 0; j < numOutChannels; j++) {
            ChannelOut * out = DatabusGetOutChannel(db, j);
            ObjectList * conns = out->GetConnections(out);

            for (k = 0; k < conns->Size(conns); k++) {
                Connection * connection = (Connection *) conns->At(conns, k);
                if (connection->GetInfo(connection) == info) {
                    return connection;
                }
            }
        }
    }

    return NULL;
}

static void DestroyOneComponent(Component * comp) {
    McxTime time_destroy_begin, time_destroy_end;
    McxTime time_diff;
    double wall_time_sec;

    ComponentLog(comp, LOG_DEBUG, "Finishing");
    mcx_time_get(&time_destroy_begin);

    object_destroy(comp);

    mcx_time_get(&time_destroy_end);
    mcx_time_diff(&time_destroy_begin, &time_destroy_end, &time_diff);
    wall_time_sec = mcx_time_to_seconds(&time_diff);
    mcx_log(LOG_DEBUG, "Finished in %fs (Wall-Time)", wall_time_sec);
}

static void ModelDestructor(void * self) {
    Model * model = (Model *) self;
    ObjectContainer * comps = model->components;
    size_t i = 0;

    if (0 == model)
        return;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        DestroyOneComponent(comp);
    }

    object_destroy(model->components);
    object_destroy(model->connections);
    object_destroy(model->factory);

    object_destroy(model->subModelGenerator);
    object_destroy(model->initialSubModelGenerator);
    object_destroy(model->subModel);
    object_destroy(model->initialSubModel);
}

static McxStatus ModelReadComponents(void * self, ComponentsInput * input) {
    Model * model = (Model *) self;

    ObjectContainer * comps = model->components;

    size_t i = 0;

    size_t numComps = input->components->Size(input->components);
    McxStatus retVal = RETURN_OK;

    mcx_log(LOG_INFO, "Reading model elements");

    // loop over all components here
    for (i = 0; i < numComps; i++) {
        Component * comp = NULL;
        ComponentInput * componentInput = (ComponentInput *) input->components->At(input->components, i);
        InputElement * componentElement = (InputElement *) componentInput;

        comp = CreateComponentFromComponentInput(model->factory, componentInput, i, model->config);
        if (NULL == comp) { // error message in function
            return RETURN_ERROR;
        }
        comp->SetModel(comp, model);

        // Finished reading component, add to list
        retVal = comps->PushBack(comps, (Object *) comp);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Model: Adding component to component list failed");
            return RETURN_ERROR;
        }

        // add the name of the component to the string list
        retVal = comps->SetElementName(comps, i, comp->GetName(comp));
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Model: Adding component name to component list failed");
            return RETURN_ERROR;
        }
    }

    mcx_log(LOG_INFO, "Read %d elements", numComps);
    mcx_log(LOG_INFO, " ");

    return RETURN_OK;
}

McxStatus ReadConnections(Vector * connections,
                          ConnectionsInput * connectionsInput,
                          ObjectContainer * components,
                          Component * sourceComp,
                          Component * targetComp) {
    size_t i = 0;
    ObjectContainer * connInputs = connectionsInput->connections;

    // loop over all connections here
    for (i = 0; i < connInputs->Size(connInputs); i++) {
        ConnectionInput * connInput = (ConnectionInput*)connInputs->At(connInputs, i);
        Vector * conns = NULL;
        size_t connsSize = 0;
        size_t j = 0;

        conns = ConnectionInfoFactoryCreateConnectionInfos(components, connInput, sourceComp, targetComp);
        if (NULL == conns) {
            mcx_log(LOG_ERROR, "Model: Could not read connection information");
            return RETURN_ERROR;
        }

        connsSize = conns->Size(conns);
        for (j = 0; j < connsSize; j++) {
            ConnectionInfo * info = (ConnectionInfo *) conns->At(conns, j);
            char * connStr = NULL;

            connStr = ConnectionInfoConnectionString(info);
            mcx_log(LOG_DEBUG, "  Connection: %s", connStr);
            mcx_free(connStr);
        }

        connections->Append(connections, conns);
        object_destroy(conns);
    }

    mcx_log(LOG_INFO, "Read %d connections", connections->Size(connections));
    mcx_log(LOG_INFO, " ");

    return RETURN_OK;
}

static McxStatus ModelReadConnections(void * self, ConnectionsInput * input) {
    Model * model = (Model *) self;

    if (!input) {
        mcx_log(LOG_WARNING, "Model: No connections specified");
        return RETURN_WARNING;
    }

    mcx_log(LOG_INFO, "Reading model connections");
    return ReadConnections(model->connections, input, model->components, NULL, NULL);
}

static McxStatus ModelCheckConnectivity(Model * model) {
    return CheckConnectivity(model->connections);
}

McxStatus MakeConnections(Vector * connections, InterExtrapolatingType isInterExtrapolating) {
    ConnectionInfo * info = NULL;
    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    for (i = 0; i < connections->Size(connections); i++) {
        info = (ConnectionInfo *) connections->At(connections, i);
        retVal = MakeOneConnection(info, isInterExtrapolating);
        if (RETURN_OK != retVal) {
            return retVal;
        }
    }

    return RETURN_OK;
}

static McxStatus ModelMakeConnections(Model * model) {
    Task * task = model->task;
    StepTypeType type = task->GetStepTypeType(task);

    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    InterExtrapolatingType isInterExtrapolating = INTERPOLATING;

    if (STEP_TYPE_SEQUENTIAL == type) {
        isInterExtrapolating = INTERPOLATING;
    } else {
        isInterExtrapolating = EXTRAPOLATING;
    }

    retVal = MakeConnections(model->connections, isInterExtrapolating);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Creating connection %d failed", i);
        return RETURN_ERROR;
    }
    mcx_log(LOG_DEBUG, "Creating connections done");

    return RETURN_OK;
}

static McxStatus ModelRead(void * self, ModelInput * input) {
    Model * model = (Model *) self;
    McxStatus retVal = RETURN_OK;

    if (!model->config || !model->task) {
        mcx_log(LOG_DEBUG, "Config or Task are not set in Model");
        return RETURN_ERROR;
    }

    retVal = model->ReadComponents(model, input->components);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Reading elements failed");
        return RETURN_ERROR;
    }

    retVal = model->ReadConnections(model, input->connections);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Reading connections failed");
        return RETURN_ERROR;
    }

    retVal = ModelPreprocess(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Preprocessing model failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ModelDoComponentNameCheck(Component * comp, void * param) {
    Component * comp2 = (Component *) param;
    if (! strcmp(comp->GetName(comp), comp2->GetName(comp2)) && (comp != comp2)) {
        mcx_log(LOG_ERROR, "Model: Elements %d and %d have the same name", comp->GetID(comp), comp2->GetID(comp2));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ModelDoComponentConsistencyChecks(Component * comp, void * param) {
    SubModel * subModel = comp->GetModel(comp)->subModel;

    McxStatus retVal = RETURN_OK;
    size_t i = 0;

    struct Databus * db = comp->GetDatabus(comp);

    size_t numInChannels  = DatabusGetInChannelsNum(db);
    size_t numOutChannels = DatabusGetOutChannelsNum(db);

    retVal = subModel->LoopComponents(subModel, ModelDoComponentNameCheck, comp);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Element names not unique");
        return RETURN_ERROR;
    }

    for (i = 0; i < numInChannels; i++) {
        Channel * channel = (Channel *)DatabusGetInChannel(db, i);
        ChannelIn * in = (ChannelIn *) channel;
        ChannelInfo * info = &channel->info;

        if (info->mode == CHANNEL_MANDATORY && !channel->ProvidesValue(channel)) {
            mcx_log(LOG_ERROR, "Model: %d. inport (%s) of element %s not connected", i+1, ChannelInfoGetName(info), comp->GetName(comp));
            return RETURN_ERROR;
        }
    }

    for (i = 0; i < numOutChannels; i++) {
        Channel * channel = (Channel *)DatabusGetOutChannel(db, i);
        ChannelInfo * info = &channel->info;

        if (info->mode == CHANNEL_MANDATORY && !channel->ProvidesValue(channel)) {
            mcx_log(LOG_ERROR, "Model: %d. outport (%s) of element %s not connected", i+1, ChannelInfoGetName(info), comp->GetName(comp));
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus ModelDoConsistencyChecks(Model * model) {
    SubModel * subModel = model->subModel;

    Vector * conns = model->connections;
    ObjectContainer * comps = model->components;

    Task * task = model->task;
    StepTypeType type = task->GetStepTypeType(task);

    McxStatus retVal = RETURN_OK;

    size_t i = 0;

    int hasDecoupleInfos = 0;
    int hasTriggerSequence = 0;

    /* check if model has decouple infos */
    for (i = 0; i < conns->Size(conns); i++) {
        ConnectionInfo * info = (ConnectionInfo *) conns->At(conns, i);

        if (info->decouplePriority > 0 || info->decoupleType != DECOUPLE_DEFAULT) {
            hasDecoupleInfos = 1;
        }
    }

    /* check if trigger sequences are present */
    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);

        if (comp->GetSequenceNumber(comp) > -1) {
            hasTriggerSequence = 1;
        }
    }

    if (hasDecoupleInfos && hasTriggerSequence) {
        mcx_log(LOG_WARNING, "Model: Both Decoupling Information and Trigger Sequences are present: ignoring Decoupling Information");
    }

    retVal = subModel->LoopComponents(subModel, ModelDoComponentConsistencyChecks, NULL);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Inconsistencies found");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus ModelConnectionsEnterInitMode(ObjectContainer * comps) {
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        McxStatus retVal = ComponentOutConnectionsEnterInitMode(comp);
        if (retVal == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "Model: Could not enter initialization mode for element %s", comp->GetName(comp));
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus ModelConnectionsExitInitMode(ObjectContainer * comps, double time) {
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        McxStatus retVal = ComponentOutConnectionsExitInitMode(comp, time);
        if (retVal == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "Model: Could not exit initialization mode for element %s", comp->GetName(comp));
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus ModelDoConnectionsInitialization(ObjectContainer * comps, int onlyIfDecoupled) {
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        Component * comp = (Component *) comps->At(comps, i);
        McxStatus retVal = ComponentDoOutConnectionsInitialization(comp, onlyIfDecoupled);
        if (retVal == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "Model: Could initialize connections from element %s", comp->GetName(comp));
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static size_t ModelGetNumOutChannels(const Model * model) {
    size_t count = 0;
    ObjectContainer * comps = model->components;
    Component * comp = NULL;
    size_t i = 0;

    for (i = 0; i < comps->Size(comps); i++) {
        comp = (Component *) comps->At(comps, i);
        count += comp->GetNumOutChannels(comp);
    }

    return count;
}

static McxStatus ModelPrint(Model * model) {
    size_t i = 0;

    PrintModelGraph(model->components, model->connections, NULL, NULL, NULL, "model.dot");

    for (i = 0; i < model->components->Size(model->components); i++) {
        Component * comp = (Component *) model->components->At(model->components, i);
        char * filename = (char *) mcx_calloc(sizeof(char), strlen(comp->GetName(comp)) + strlen(".dot") + 1);

        sprintf(filename, "%s.dot", comp->GetName(comp));
        PrintComponentGraph(comp, filename, comp->GetInOutGroupsDependency(comp), RUNTIME_DEPENDENCIES);
        mcx_free(filename);

        if ( comp->GetInOutGroupsInitialDependency ) {
            char * initialFilename = (char *) mcx_calloc(sizeof(char), strlen(comp->GetName(comp)) + strlen("_initial.dot") + 1);
            struct Dependencies * A = comp->GetInOutGroupsInitialDependency(comp);

            sprintf(initialFilename, "%s_initial.dot", comp->GetName(comp));
            PrintComponentGraph(comp, initialFilename, A, INITIAL_DEPENDENCIES);

            DependenciesDestroy(A);
            mcx_free(initialFilename);
        }
    }
    return RETURN_OK;
}

static McxStatus SetupOneComponent(Component * comp) {
    McxStatus retVal = RETURN_OK;
    McxStatus status = RETURN_OK;

    // setup components
    mcx_log(LOG_DEBUG, "  Element: \"%s\"", comp->GetName(comp));
    status = ComponentSetup(comp);
    if (RETURN_ERROR == status) {
        mcx_log(LOG_ERROR, "Model: Generic setup of element %s failed", comp->GetName(comp));
        return RETURN_ERROR;
    } else if (RETURN_WARNING == status) {
        mcx_log(LOG_WARNING, "Model: Generic setup of element %s returned with a warning", comp->GetName(comp));
        retVal = status;
    }

    if (comp->Setup) {
        mcx_signal_handler_set_name(comp->GetName(comp));

        status = comp->Setup(comp);
        if (RETURN_ERROR == status) {
            mcx_log(LOG_ERROR, "Model: Setup of element %s failed", comp->GetName(comp));
            mcx_signal_handler_unset_name();
            return RETURN_ERROR;
        } else if (RETURN_WARNING == status) {
            mcx_log(LOG_WARNING, "Model: Setup of element %s returned with a warning", comp->GetName(comp));
            if (RETURN_ERROR != retVal) {
                retVal = status;
            }
        }
        mcx_signal_handler_unset_name();
    }

    return retVal;
}

McxStatus SetupComponents(ObjectContainer * components, Component * leaveOutComponent) {
    size_t i = 0;
    McxStatus retVal = RETURN_OK;

    // loop over all components here
    for (i = 0; i < components->Size(components); i++) {
        Component * comp = (Component *) components->At(components, i);

        if (comp != leaveOutComponent) {
            McxStatus status = SetupOneComponent(comp);

            if (RETURN_ERROR == status) {
                return RETURN_ERROR;
            } else if (RETURN_WARNING == status) {
                if (RETURN_ERROR != retVal) {
                    retVal = status;
                }
            }
        }
    }

    return retVal;
}

McxStatus SetupDatabusComponents(ObjectContainer * components) {
    Component * comp = NULL;
    size_t i = 0;
    McxStatus retVal = RETURN_OK;
    McxStatus status = RETURN_OK;

    // loop over all components here
    for (i = 0; i < components->Size(components); i++) {
        comp = (Component *) components->At(components, i);

        // setup components
        mcx_log(LOG_DEBUG, "  Element: \"%s\"", comp->GetName(comp));
        if (comp->SetupDatabus) {
            mcx_signal_handler_set_name(comp->GetName(comp));
            status = comp->SetupDatabus(comp);
            if (RETURN_ERROR == status) {
                mcx_log(LOG_ERROR, "Model: Setting up ports for element %s failed", comp->GetName(comp));
                retVal = status;
            } else if (RETURN_WARNING == status) {
                mcx_log(LOG_WARNING, "Model: Setting up ports for element %s returned with a warning");
                if (RETURN_ERROR != retVal) {
                    retVal = status;
                }
            }
            mcx_signal_handler_unset_name();
        }
    }
    return retVal;
}

static McxStatus ModelSetup(void * self) {
    Model * model = (Model *) self;

    McxStatus retVal = RETURN_OK;

    mcx_log(LOG_DEBUG, "Checking model connections");
    retVal = ModelCheckConnectivity(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Checking model connections failed");
        return RETURN_ERROR;
    }
    mcx_log(LOG_DEBUG, " ");

    mcx_log(LOG_INFO, "Setting up model elements");

    retVal = SetupComponents(model->components
                             , NULL
        );
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Setting up model elements failed");
        return RETURN_ERROR;
    }
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "Setting up data bus of model elements");
    retVal = SetupDatabusComponents(model->components);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Setting up of data bus of model elements failed");
        return RETURN_ERROR;
    }
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "Setting up model connections");
    retVal = ModelMakeConnections(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Setting up model connections failed");
        return RETURN_ERROR;
    }
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "Calculating model elements evaluation order:");
    retVal = ModelConnectionsDone(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Calculating model elements evaluation order failed");
        return RETURN_ERROR;
    }
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_DEBUG, "Setting up model connection filters:");
    retVal = ModelInsertAllFilters(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Setting up model connection filters");
        return RETURN_ERROR;
    }
    mcx_log(LOG_DEBUG, " ");

    mcx_log(LOG_DEBUG, "Process model consistency checks");
    retVal = ModelDoConsistencyChecks(model);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Model consistency checks failed");
        return RETURN_ERROR;
    }
    mcx_log(LOG_DEBUG, " ");

    if (model->config->outputModel) {
        retVal = ModelPrint(model);
        if (RETURN_OK != retVal) {
            /* do not abort run just because graph could not be exported */
        }
    }

    return RETURN_OK;
}

static McxStatus CompInit(Component * comp, void * param) {
    const Task * task  = (const Task *) param;

    /* no time passes during initialization -> .startTime == .endTime */
    double startTime = comp->GetTime(comp);
    TimeInterval time = {startTime, startTime};

    McxStatus retVal = RETURN_OK;

    retVal = DatabusTriggerInConnections(comp->GetDatabus(comp), &time);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Updating inports during initialization failed");
        return RETURN_ERROR;
    }

    retVal = ComponentInitialize(comp, 0, task->params->time);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Generic initialize failed");
        return RETURN_ERROR;
    }

    if (comp->Initialize) {
        mcx_signal_handler_set_name(comp->GetName(comp));
        retVal = comp->Initialize(comp, 0, task->params->time);
        mcx_signal_handler_unset_name();

        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Model: Element initialize failed");
            return RETURN_ERROR;
        }
    }

    retVal = ComponentUpdateOutChannels(comp, &time);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Updating outports during initialization failed");
        return RETURN_ERROR;
    }

    return retVal;
}

static McxStatus CompExitInit(Component * comp, void * param) {
    return comp->ExitInitializationMode(comp);
}

static McxStatus CompUpdateInitOutputs(CompAndGroup * compGroup, void * param) {
    Component  * comp  = compGroup->comp;
    const Task * task  = (const Task *) param;

    /* no time passes during initialization -> .startTime == .endTime */
    double startTime = comp->GetTime(comp);
    TimeInterval time = {startTime, startTime};

    McxStatus retVal = RETURN_OK;

    retVal = DatabusTriggerInConnections(comp->GetDatabus(comp), &time);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Updating inports during initialization loop failed");
        return RETURN_ERROR;
    }

    if (comp->GetModel(comp)->config->cosimInitEnabled) {
        if (comp->UpdateInChannels) {
            retVal = comp->UpdateInChannels(comp);
            if (retVal != RETURN_OK) {
                return retVal;
            }
        }

        mcx_signal_handler_set_name(comp->GetName(comp));

        retVal = comp->UpdateInitialOutChannels(comp);

        mcx_signal_handler_unset_name();

        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Model: Updating initial outports during initialization loop failed");
            return RETURN_ERROR;
        }
    }

    retVal = ComponentUpdateOutChannels(comp, &time);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Updating outports during initialization loop failed");
        return RETURN_ERROR;
    }

    return retVal;
}

static McxStatus CompUpdateOutputs(CompAndGroup * compGroup, void * param) {
    Component  * comp  = compGroup->comp;
    const Task * task  = (const Task *) param;

    TimeInterval time = {task->params->time, task->params->time};

    McxStatus retVal = RETURN_OK;

    retVal = ComponentUpdateOutChannels(comp, &time);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Updating outports failed");
        return RETURN_ERROR;
    }

    return retVal;
}

static McxStatus ModelInitialize(Model * model) {
    SubModel * subModel = model->config->cosimInitEnabled ? model->initialSubModel : model->subModel;
    McxStatus retVal = RETURN_OK;

    // enter initialization mode
    retVal = ModelConnectionsEnterInitMode(model->components);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Entering initialization mode failed");
        return RETURN_ERROR;
    }

    retVal = ModelDoConnectionsInitialization(model->components, TRUE);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Initialization of connections failed");
        return RETURN_ERROR;
    }

    retVal = model->subModel->LoopComponents(model->subModel, CompInit, (void *) model->task);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Initialization of elements failed");
        return RETURN_ERROR;
    }

    // initialization
    retVal = subModel->LoopEvaluationList(subModel, CompUpdateInitOutputs, (void *) model->task);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Initialization of elements failed");
            return RETURN_ERROR;
    }

    // exit initialization mode
    retVal = subModel->LoopComponents(subModel, CompExitInit, NULL);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Initialization of elements failed");
        return retVal;
    }

    retVal = ModelConnectionsExitInitMode(model->components, model->task->params->time);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Exiting initialization mode failed");
        return RETURN_ERROR;
    }

    // update outputs for inputs for first DoStep
    retVal = subModel->LoopEvaluationList(subModel, CompUpdateOutputs, (void *) model->task);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Model: Initialization of elements failed");
            return RETURN_ERROR;
    }

    {
        McxTime rtGlobalSimStart;
        mcx_time_get(&rtGlobalSimStart);

        retVal = subModel->LoopComponents(subModel, ComponentBeforeDoSteps, &rtGlobalSimStart);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Model: Initialization of elements failed");
            return retVal;
        }
    }

    return RETURN_OK;
}

static const char * GetComponentAbbrev(const char * type) {
    if (!strcmp(type, compConstantTypeString)) {
        return "C";
    } else if (!strcmp(type, "INTEGRATOR")) {
        return "INT";
    } else {
        return type;
    }
}

McxStatus PrintComponentGraph(Component * comp,
                              const char * filename,
                              struct Dependencies * A,
                              DependencyType depType) {
    const char * tableArgs = "border=\"0\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\"";

    size_t i = 0;
    size_t j = 0;
    size_t grp = 0;

    Databus * db = comp->GetDatabus(comp);

    if (NULL == A) {
        ComponentLog(comp, LOG_ERROR, "Could not create dependencies for component graph");
        return RETURN_ERROR;
    }
    FILE * dotFile = mcx_os_fopen(filename, "w");

    mcx_os_fprintf(dotFile, "digraph model {\n");
    mcx_os_fprintf(dotFile, "graph [nodesep=\"1\"]\n");

    mcx_os_fprintf(dotFile, "node [shape=none, label=<\n");
    mcx_os_fprintf(dotFile, "<table %s>\n", tableArgs);
    mcx_os_fprintf(dotFile, "  <tr>\n");
    mcx_os_fprintf(dotFile, "    <td>in</td>\n");
    mcx_os_fprintf(dotFile, "    <td>\n");
    /* input channels */
    if (GetDependencyNumIn(A) > 0) {
        mcx_os_fprintf(dotFile, "      <table %s>\n", tableArgs);
        for (j = 0; j < GetDependencyNumIn(A); j++) {
            mcx_os_fprintf(dotFile, "        <tr>\n");
            mcx_os_fprintf(dotFile, "          <td port=\"in%d\">%d</td>\n", j, j);
            mcx_os_fprintf(dotFile, "        </tr>\n");
        }
        mcx_os_fprintf(dotFile, "      </table>\n");
    }
    mcx_os_fprintf(dotFile, "    </td>\n");
    mcx_os_fprintf(dotFile, "  </tr>\n");
    mcx_os_fprintf(dotFile, "</table>>] in;\n");

    mcx_os_fprintf(dotFile, "node [shape=none, label=<\n");
    mcx_os_fprintf(dotFile, "<table %s>\n", tableArgs);
    mcx_os_fprintf(dotFile, "  <tr>\n");
    mcx_os_fprintf(dotFile, "    <td>\n");
    /* output channels */
    if (DatabusGetOutChannelsNum(db) > 0) {
        mcx_os_fprintf(dotFile, "      <table %s>\n", tableArgs);
        for (j = 0; j < DatabusGetOutChannelsNum(db); j++) {
            mcx_os_fprintf(dotFile, "        <tr>\n");
            mcx_os_fprintf(dotFile, "          <td port=\"out%d\">%d</td>\n", j, j);
            mcx_os_fprintf(dotFile, "        </tr>\n");
        }
        mcx_os_fprintf(dotFile, "      </table>\n");
    }
    mcx_os_fprintf(dotFile, "    </td>\n");
    mcx_os_fprintf(dotFile, "    <td>out</td>\n");
    mcx_os_fprintf(dotFile, "  </tr>\n");
    mcx_os_fprintf(dotFile, "</table>>] out;\n");

    mcx_os_fprintf(dotFile, "{\n");
    mcx_os_fprintf(dotFile, "rank = same;\n");
    mcx_os_fprintf(dotFile, "rankdir = LR;\n");

    for (i = 0; i < GetDependencyNumIn(A); i++) {
        for (j = 0; j < DatabusGetOutChannelsNum(db); j++) {
            Dependency dep;
            if (INITIAL_DEPENDENCIES == depType) {
                grp = comp->GetInitialOutGroup(comp, j);
            } else {
                grp = comp->GetOutGroup(comp, j);
            }
            GetDependency(A, i, grp, &dep);
            if (dep != DEP_INDEPENDENT) {
                mcx_os_fprintf(dotFile, "in:in%d -> out:out%d;\n", i, j);
            }
        }
    }

    mcx_os_fprintf(dotFile, "}\n");

    mcx_os_fprintf(dotFile, "labelloc=\"t\";\n");
    mcx_os_fprintf(dotFile, "label=\"%s (%s)\";\n", GetComponentAbbrev(comp->GetType(comp)), comp->GetName(comp));

    mcx_os_fprintf(dotFile, "}\n");

    fflush(dotFile);
    mcx_os_fclose(dotFile);

    return RETURN_OK;
}

McxStatus PrintModelGraph(ObjectContainer * comps, Vector * conns, Component * inComp, Component * outComp, const char * title, const char * filename) {
    size_t i = 0;
    size_t j = 0;

    FILE * dotFile = mcx_os_fopen(filename, "w");

    const char * tableArgs = "border=\"0\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\"";

    mcx_os_fprintf(dotFile, "digraph model {\n");
    mcx_os_fprintf(dotFile, "graph [nodesep=\"1\"]\n");

    if (inComp) {
        Databus * db = inComp->GetDatabus(inComp);

        mcx_os_fprintf(dotFile, "node [shape=none, label=<\n");
        mcx_os_fprintf(dotFile, "<table %s>\n", tableArgs);
        mcx_os_fprintf(dotFile, "  <tr>\n");
        mcx_os_fprintf(dotFile, "    <td>in</td>\n");
        mcx_os_fprintf(dotFile, "    <td>\n");
        /* output channels */
        if (DatabusGetOutChannelsNum(db) > 0) {
        mcx_os_fprintf(dotFile, "      <table %s>\n", tableArgs);
        for (j = 0; (int) j < DatabusGetOutChannelsNum(db); j++) {
        mcx_os_fprintf(dotFile, "        <tr>\n");
        mcx_os_fprintf(dotFile, "          <td port=\"out%d\">%d</td>\n", j, j);
        mcx_os_fprintf(dotFile, "        </tr>\n");
        }
        mcx_os_fprintf(dotFile, "      </table>\n");
        }
        mcx_os_fprintf(dotFile, "    </td>\n");
        mcx_os_fprintf(dotFile, "  </tr>\n");
        mcx_os_fprintf(dotFile, "</table>>] comp%d;\n", comps->Size(comps));
    }

    for (i = 0; i < comps->Size(comps); i++) {
        Component * c = (Component *) comps->At(comps, i);
        Databus * db = c->GetDatabus(c);

        mcx_os_fprintf(dotFile, "node [shape=none, label=<\n");
        mcx_os_fprintf(dotFile, "<table %s>\n", tableArgs);
        mcx_os_fprintf(dotFile, "  <tr>\n");
        mcx_os_fprintf(dotFile, "    <td>\n");
        /* input channels */
        if (DatabusGetInChannelsNum(db) > 0) {
        mcx_os_fprintf(dotFile, "      <table %s>\n", tableArgs);
        for (j = 0; (int) j < DatabusGetInChannelsNum(db); j++) {
        mcx_os_fprintf(dotFile, "        <tr>\n");
        mcx_os_fprintf(dotFile, "          <td port=\"in%d\">%d</td>\n", j, j);
        mcx_os_fprintf(dotFile, "        </tr>\n");
        }
        mcx_os_fprintf(dotFile, "      </table>\n");
        }
        mcx_os_fprintf(dotFile, "    </td>\n");
        /* component description */
        mcx_os_fprintf(dotFile, "    <td>%s<br/><font point-size=\"8\">(%s)</font></td>\n", GetComponentAbbrev(c->GetType(c)), c->GetName(c));
        /* output channels */
        mcx_os_fprintf(dotFile, "    <td>\n");
        if (DatabusGetOutChannelsNum(db) > 0) {
        mcx_os_fprintf(dotFile, "      <table %s>\n", tableArgs);
        for (j = 0; (int) j < DatabusGetOutChannelsNum(db); j++) {
        mcx_os_fprintf(dotFile, "        <tr>\n");
        mcx_os_fprintf(dotFile, "          <td port=\"out%d\">%d</td>\n", j, j);
        mcx_os_fprintf(dotFile, "        </tr>\n");
        }
        mcx_os_fprintf(dotFile, "      </table>\n");
        }
        mcx_os_fprintf(dotFile, "    </td>\n");
        mcx_os_fprintf(dotFile, "  </tr>\n");
        mcx_os_fprintf(dotFile, "</table>>] comp%d;\n", c->GetID(c));
    }

    if (outComp) {
        Databus * db = outComp->GetDatabus(outComp);

        mcx_os_fprintf(dotFile, "node [shape=none, label=<\n");
        mcx_os_fprintf(dotFile, "<table %s>\n", tableArgs);
        mcx_os_fprintf(dotFile, "  <tr>\n");
        mcx_os_fprintf(dotFile, "    <td>\n");
        /* output channels */
        if (DatabusGetInChannelsNum(db) > 0) {
        mcx_os_fprintf(dotFile, "      <table %s>\n", tableArgs);
        for (j = 0; (int) j < DatabusGetInChannelsNum(db); j++) {
        mcx_os_fprintf(dotFile, "        <tr>\n");
        mcx_os_fprintf(dotFile, "          <td port=\"in%d\">%d</td>\n", j, j);
        mcx_os_fprintf(dotFile, "        </tr>\n");
        }
        mcx_os_fprintf(dotFile, "      </table>\n");
        }
        mcx_os_fprintf(dotFile, "    </td>\n");
        mcx_os_fprintf(dotFile, "    <td>out</td>\n");
        mcx_os_fprintf(dotFile, "  </tr>\n");
        mcx_os_fprintf(dotFile, "</table>>] comp%d;\n", comps->Size(comps) + 1);
    }

    mcx_os_fprintf(dotFile, "{\n");

    if (inComp && outComp) {
        mcx_os_fprintf(dotFile, "comp%d -> comp%d [style=invis];\n",
                comps->Size(comps),
                comps->Size(comps) + 1);
    }
    for (i = 0; i < conns->Size(conns); i++) {
        ConnectionInfo * info = (ConnectionInfo *) conns->At(conns, i);

        Component * src = info->sourceComponent;
        Component * trg = info->targetComponent;

        size_t srcID = src->GetID(src);
        size_t trgID = trg->GetID(trg);

        if (src == inComp) {
            srcID = comps->Size(comps);
        }
        if (trg == outComp) {
            trgID = comps->Size(comps) + 1;
        }

        mcx_os_fprintf(dotFile, "comp%zu:out%d -> comp%zu:in%d;\n",
                srcID, info->sourceChannel,
                trgID, info->targetChannel);
    }

    mcx_os_fprintf(dotFile, "}\n");
    mcx_os_fprintf(dotFile, "rankdir = LR;\n");
    if (title) {
        mcx_os_fprintf(dotFile, "labelloc=\"t\";\n");
        mcx_os_fprintf(dotFile, "label=\"%s\";\n", title);
    }

    mcx_os_fprintf(dotFile, "}\n");

    fflush(dotFile);
    mcx_os_fclose(dotFile);

    return RETURN_OK;
}

static void ModelSetConfig(Model * model, Config * config) {
    model->config = config;
}

static void ModelSetTask(Model * model, Task * task) {
    model->task = task;
}

static void ModelSetComponentFactory(Model * model, ComponentFactory * factory) {
    model->factory = factory;
}

static Task * ModelGetTask(Model * model) {
    return model->task;
}

static Model * ModelCreate(Model * model) {
    /* set function pointers */

    model->ReadComponents = ModelReadComponents;
    model->ReadConnections = ModelReadConnections;

    model->Read = ModelRead;
    model->Setup = ModelSetup;
    model->Initialize = ModelInitialize;
    model->SetConfig = ModelSetConfig;
    model->SetTask   = ModelSetTask;
    model->SetComponentFactory = ModelSetComponentFactory;

    model->GetTask = ModelGetTask;

    // set to default values
    model->components = (ObjectContainer *) object_create(ObjectContainer);
    model->connections = (Vector *) object_create(Vector);
    model->connections->Setup(model->connections, sizeof(ConnectionInfo), ConnectionInfoInit, NULL, NULL);
    model->factory = NULL;

    model->config = NULL;
    model->task = NULL;

    model->subModelGenerator = (SubModelGenerator *) object_create(SubModelGenerator);
    model->initialSubModelGenerator = (SubModelGenerator *) object_create(SubModelGenerator);
    model->subModel = NULL;
    model->initialSubModel = NULL;

    return model;
}

OBJECT_CLASS(Model, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */