/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/Databus.h"
#include "core/connections/filters/Filter.h"

#include "core/Model.h"
#include "reader/model/ports/PortsInput.h"
#include "reader/model/ports/PortInput.h"

#include "core/channels/ChannelInfo.h"
#include "core/channels/Channel.h"
#include "core/connections/Connection.h"
#include "core/connections/ConnectionInfo.h"

#include "core/connections/FilteredConnection.h"

#include "objects/Vector.h"

#include "util/stdlib.h"

// private headers, see "Object-oriented programming in ANSI-C", Hanser 1994
#include "core/Databus_impl.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// DatabusInfo

static void DatabusInfoDataDestructor(DatabusInfoData * data) {
    object_destroy(data->infos);
}

static DatabusInfoData * DatabusInfoDataCreate(DatabusInfoData * data) {
    data->infos = (Vector *) object_create(Vector);
    data->infos->Setup(data->infos, sizeof(ChannelInfo), ChannelInfoInit, ChannelInfoSetFrom, ChannelInfoDestroy);
    return data;
}

OBJECT_CLASS(DatabusInfoData, Object);


static ChannelInfo * DatabusReadPortInput(PortInput * input) {
    McxStatus retVal = RETURN_OK;

    ChannelInfo * info = (ChannelInfo *)mcx_calloc(1, sizeof(ChannelInfo));
    if (!info) {
        return NULL;
    }

    retVal = ChannelInfoInit(info);
    if (RETURN_ERROR == retVal) {
        goto cleanup_0;
    }

    if (input->type == PORT_VECTOR) {
        VectorPortInput * vectorPortInput = input->port.vectorPort;
        InputElement * vectorPortElement = (InputElement *) vectorPortInput;
        ChannelDimension * dimension = NULL;

        int64_t startIdx = 0;
        int64_t endIdx   = 0;

        startIdx = vectorPortInput->startIndex;
        if (startIdx < 0) {
            input_element_error(vectorPortElement, "start index must not be smaller than 0");
            retVal = RETURN_ERROR;
            goto vector_cleanup_0;
        }

        endIdx = vectorPortInput->endIndex;
        if (endIdx < startIdx) {
            input_element_error(vectorPortElement, "end index must not be smaller than start index");
            retVal = RETURN_ERROR;
            goto vector_cleanup_0;
        }

        dimension = ChannelDimensionCreate1D((size_t) startIdx, (size_t) endIdx);
        if (NULL == dimension) {
            mcx_log(LOG_ERROR, "Could not create ChannelDimension");
            retVal = RETURN_ERROR;
            goto vector_cleanup_0;
        }

        size_t dims[1] = { endIdx - startIdx + 1 };

        if (RETURN_OK != ChannelInfoSetup(info,
                                          vectorPortInput->name,
                                          vectorPortInput->nameInModel,
                                          vectorPortInput->description,
                                          vectorPortInput->unit,
                                          ChannelTypeArray(vectorPortInput->type, 1, dims),
                                          vectorPortInput->id,
                                          dimension)) {
            mcx_log(LOG_ERROR, "Could not Init ChannelInfo");
            retVal = RETURN_ERROR;
            goto vector_cleanup_0;
        }

        if (vectorPortInput->min) {
            info->min = ChannelValueNewArray(1, dims, vectorPortInput->type, vectorPortInput->min);
            if (!info->min) {
                retVal = RETURN_ERROR;
                goto vector_cleanup_0;
            }
        }

        if (vectorPortInput->max) {
            info->max = ChannelValueNewArray(1, dims, vectorPortInput->type, vectorPortInput->max);
            if (!info->max) {
                retVal = RETURN_ERROR;
                goto vector_cleanup_0;
            }
        }

        if (vectorPortInput->scale) {
            info->scale = ChannelValueNewArray(1, dims, vectorPortInput->type, vectorPortInput->scale);
            if (!info->scale) {
                retVal = RETURN_ERROR;
                goto vector_cleanup_0;
            }
        }

        if (vectorPortInput->offset) {
            info->offset = ChannelValueNewArray(1, dims, vectorPortInput->type, vectorPortInput->offset);
            if (!info->offset) {
                retVal = RETURN_ERROR;
                goto vector_cleanup_0;
            }
        }

        if (vectorPortInput->initial) {
            info->initialValue = ChannelValueNewArray(1, dims, vectorPortInput->type, vectorPortInput->initial);
            if (!info->initialValue) {
                retVal = RETURN_ERROR;
                goto vector_cleanup_0;
            }
        }

        if (vectorPortInput->default_) {
            info->defaultValue = ChannelValueNewArray(1, dims, vectorPortInput->type, vectorPortInput->default_);
            if (!info->defaultValue) {
                retVal = RETURN_ERROR;
                goto vector_cleanup_0;
            }
        }

        if (vectorPortInput->writeResults.defined) {
            info->writeResult = vectorPortInput->writeResults.value;
        }

    vector_cleanup_0:
        if (retVal == RETURN_ERROR) {
            goto cleanup_0;
        }
    } else {
        ScalarPortInput * scalarPortInput = input->port.scalarPort;

        retVal = ChannelInfoInit(info);
        if (RETURN_ERROR == retVal) {
            goto cleanup_else_1;
        }

        ChannelInfoSetName(info, scalarPortInput->name);
        ChannelInfoSetNameInTool(info, scalarPortInput->nameInModel);
        ChannelInfoSetDescription(info, scalarPortInput->description);
        ChannelInfoSetID(info, scalarPortInput->id);
        ChannelInfoSetType(info, scalarPortInput->type);

        if (!ChannelInfoIsBinary(info)) {
            ChannelInfoSetUnit(info, scalarPortInput->unit);
        } else {
            ChannelInfoSetUnit(info, "-");
        }

        ChannelType * expectedType = info->type;

        ChannelValue value;
        ChannelValueInit(&value, ChannelTypeClone(expectedType));

        if (scalarPortInput->min.defined) {
            if (RETURN_OK != ChannelValueSetFromReference(&value, &scalarPortInput->min.value)) {
                goto cleanup_else_1;
            }
            info->min = ChannelValueClone(&value);
            if (!info->min) {
                goto cleanup_else_1;
            }
        }

        if (scalarPortInput->max.defined) {
            if (RETURN_OK != ChannelValueSetFromReference(&value, &scalarPortInput->max.value)) {
                goto cleanup_else_1;
            }
            info->max = ChannelValueClone(&value);
            if (!info->max) {
                goto cleanup_else_1;
            }
        }

        if (scalarPortInput->scale.defined) {
            if (RETURN_OK != ChannelValueSetFromReference(&value, &scalarPortInput->scale.value)) {
                goto cleanup_else_1;
            }
            info->scale = ChannelValueClone(&value);
            if (!info->scale) {
                goto cleanup_else_1;
            }
        }

        if (scalarPortInput->offset.defined) {
            if (RETURN_OK != ChannelValueSetFromReference(&value, &scalarPortInput->offset.value)) {
                goto cleanup_else_1;
            }
            info->offset = ChannelValueClone(&value);
            if (!info->offset) {
                goto cleanup_else_1;
            }
        }

        if (scalarPortInput->default_.defined) {
            if (RETURN_OK != ChannelValueSetFromReference(&value, &scalarPortInput->default_.value)) {
                goto cleanup_else_1;
            }
            info->defaultValue = ChannelValueClone(&value);
            if (!info->defaultValue) {
                goto cleanup_else_1;
            }
        }

        if (scalarPortInput->initial.defined) {
            if (RETURN_OK != ChannelValueSetFromReference(&value, &scalarPortInput->initial.value)) {
                goto cleanup_else_1;
            }
            info->initialValue = ChannelValueClone(&value);
            if (!info->initialValue) {
                goto cleanup_else_1;
            }
        }

        if (scalarPortInput->writeResults.defined) {
            info->writeResult = scalarPortInput->writeResults.value;
        }

    cleanup_else_1:
        ChannelValueDestructor(&value);
        goto cleanup_0;
    }

cleanup_0:
    if (RETURN_ERROR == retVal) {
        object_destroy(info);
    }

    return info;
}

static int ChannelInfoSameNamePred(const void * elem, const char * name) {
    const ChannelInfo * info = (const ChannelInfo *) elem;

    return 0 == strcmp(ChannelInfoGetName(info), name);
}

static int ChannelInfosGetNameIdx(Vector * infos, const char * name) {
    size_t idx = infos->FindIdx(infos, ChannelInfoSameNamePred, name);

    return idx == SIZE_T_ERROR ? -1 : (int)idx;
}

int DatabusInfoGetChannelID(DatabusInfo * info, const char * name) {
    return ChannelInfosGetNameIdx(info->data->infos, name);
}

McxStatus DatabusInfoRead(DatabusInfo * dbInfo,
                          PortsInput * input,
                          fComponentChannelRead SpecificRead,
                          Component * comp,
                          ChannelMode mode) {
    size_t i = 0;
    size_t j = 0;

    McxStatus retVal = RETURN_OK;

    size_t numChildren = input->ports->Size(input->ports);

    Vector * dbInfos = dbInfo->data->infos;
    size_t requiredSize = 0;
    StringContainer * portNames = StringContainerCreate(numChildren);

    if (!portNames) {
        mcx_log(LOG_ERROR, "Ports: Port name container allocation failed");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    for (i = 0; i < numChildren; i++) {
        PortInput * portInput = (PortInput *) input->ports->At(input->ports, i);
        if (portInput->type == PORT_SCALAR) {
            requiredSize++;
        } else {
            requiredSize += portInput->port.vectorPort->endIndex - portInput->port.vectorPort->startIndex + 1;
        }

        ChannelInfo * info = DatabusReadPortInput(portInput);
        if (!info) {
            mcx_log(LOG_ERROR, "Ports: Read port infos: Could not read info of port %d", i);
            retVal = RETURN_ERROR;
            goto cleanup;
        }

        info->mode = mode;

        const char * name = ChannelInfoGetName(info);
        if (info->dimension) {
            mcx_log(LOG_DEBUG, "    Port: \"%s[%d:%d]\"", name, info->dimension->startIdxs[0], info->dimension->endIdxs[0]);
        } else {
            mcx_log(LOG_DEBUG, "    Port: \"%s\"", name);
        }

        {
            // check for duplicates
            int n = StringContainerGetIndex(portNames, name);
            if (n >= 0) {  // key already exists
                mcx_log(LOG_ERROR, "Ports: Duplicate port %s", name);
                retVal = RETURN_ERROR;
                goto cleanup;
            }
        }

        if (SpecificRead) {
            retVal = SpecificRead(comp, info, portInput, i);
            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "Ports: Read port infos: Could not read element specific data of port %d", i);
                goto cleanup;
            }
        }

        if (RETURN_OK != dbInfos->PushBack(dbInfos, info)) {
            mcx_log(LOG_ERROR, "Ports: Read port infos: Could not append info of port %d", i);
            retVal = RETURN_ERROR;
            goto cleanup;
        }

        retVal = StringContainerAddString(portNames, name);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Ports: Storing port name failed");
            goto cleanup;
        }
    }

cleanup:
    StringContainerDestroy(portNames);

    return retVal;
}

static int IsWriteResults(const void * elem, const void * ignore) {
    const ChannelInfo * info = (const ChannelInfo *) elem;

    return info->writeResult;
}

size_t DatabusInfoGetNumWriteChannels(DatabusInfo * dbInfo) {
    Vector * infos = dbInfo->data->infos;
    Vector * writeInfos = infos->FilterRef(infos, IsWriteResults, NULL);

    size_t num = writeInfos->Size(writeInfos);

    object_destroy(writeInfos);

    return num;
}

static void DatabusInfoDestructor(DatabusInfo * info) {
    object_destroy(info->data);
}

static DatabusInfo * DatabusInfoCreate(DatabusInfo * info) {
    info->data = (DatabusInfoData *) object_create(DatabusInfoData);
    if (!info->data) {
        return NULL;
    }

    return info;
}

OBJECT_CLASS(DatabusInfo, Object);


// ----------------------------------------------------------------------
// Databus

static void DatabusDataDestructor(DatabusData * data) {
    size_t i = 0;
    if (data) {
        if (data->in) {
            size_t num = DatabusInfoGetChannelNum(data->inInfo);
            for (i = 0; i < num; i++) {
                ChannelIn * in = data->in[i];
                if (in) {
                    object_destroy(in);
                }
            }
            mcx_free(data->in);
        }
        if (data->inConnected) {
            mcx_free(data->inConnected);
        }

        if (data->out) {
            size_t num = DatabusInfoGetChannelNum(data->outInfo);
            for (i = 0; i < num; i++) {
                ChannelOut * out = data->out[i];
                if (out) {
                    object_destroy(out);
                }
            }
            mcx_free(data->out);
        }

        if (data->local) {
            size_t num = DatabusInfoGetChannelNum(data->localInfo);
            for (i = 0; i < num; i++) {
                ChannelLocal * local = data->local[i];
                if (local) {
                    object_destroy(local);
                }
            }
            mcx_free(data->local);
        }

        if (data->rtfactor) {
            size_t num = DatabusInfoGetChannelNum(data->rtfactorInfo);
            for (i = 0; i < num; i++) {
                ChannelLocal * rtfactor = data->rtfactor[i];
                if (rtfactor) {
                    object_destroy(rtfactor);
                }
            }
            mcx_free(data->rtfactor);
        }

        object_destroy(data->inInfo);
        object_destroy(data->outInfo);
        object_destroy(data->localInfo);
        object_destroy(data->rtfactorInfo);
    }
}

static DatabusData * DatabusDataCreate(DatabusData * data) {
    data->in = NULL;
    data->inConnected = NULL;
    data->numInConnected = 0;

    data->out = NULL;
    data->local = NULL;
    data->rtfactor = NULL;

    data->inInfo = (DatabusInfo *) object_create(DatabusInfo);
    if (!data->inInfo) {
        return NULL;
    }

    data->outInfo = (DatabusInfo *) object_create(DatabusInfo);
    if (!data->outInfo) {
        return NULL;
    }

    data->localInfo = (DatabusInfo *) object_create(DatabusInfo);
    if (!data->localInfo) {
        return NULL;
    }

    data->rtfactorInfo = (DatabusInfo *) object_create(DatabusInfo);
    if (!data->rtfactorInfo) {
        return NULL;
    }

    return data;
}

OBJECT_CLASS(DatabusData, Object);

McxStatus DatabusUpdateInConnected(Databus * db) {
    size_t i = 0;
    size_t numIn = DatabusInfoGetChannelNum(DatabusGetInInfo(db));

    db->data->numInConnected = 0;

    for (i = 0; i < numIn; i++) {
        ChannelIn * chIn = db->data->in[i];
        Channel * ch = (Channel *) chIn;

        if (ch->IsConnected(ch)) {
            db->data->inConnected[db->data->numInConnected] = db->data->in[i];
            db->data->numInConnected++;
        }
    }

    return RETURN_OK;
}


McxStatus DatabusSetup(Databus * db, DatabusInfo * in, DatabusInfo * out, Config * config) {
    size_t i = 0;

    if (!in || !out) {
        mcx_log(LOG_ERROR, "Ports: Setup databus: No info available");
        return RETURN_ERROR;
    }
    size_t numIn = DatabusInfoGetChannelNum(in);
    size_t numOut = DatabusInfoGetChannelNum(out);

    if (numIn > 0) {
        db->data->in = (ChannelIn **) mcx_calloc(numIn, sizeof(ChannelIn *));
        if (db->data->in == NULL) {
            mcx_log(LOG_ERROR, "Ports: Memory allocation for inports failed");
            return RETURN_ERROR;
        }

        db->data->inConnected = (ChannelIn **)mcx_calloc(numIn, sizeof(ChannelIn *));
        if (db->data->inConnected == NULL) {
            mcx_log(LOG_ERROR, "Ports: Memory allocation for connected inports failed");
            return RETURN_ERROR;
        }

        for (i = 0; i < numIn; i++) {
            db->data->in[i] = (ChannelIn *) object_create(ChannelIn);
            if (!db->data->in[i]) {
                mcx_log(LOG_ERROR, "Ports: Memory allocation for inport %d failed", i);
                // cleanup
                size_t j = 0;
                for (j = 0; j < i; j++) {
                    object_destroy(db->data->in[j]);
                }
                mcx_free(db->data->in);
                db->data->in = NULL;
                return RETURN_ERROR;
            }
            db->data->in[i]->Setup(db->data->in[i], DatabusInfoGetChannel(in, i));
        }
    } else {
        db->data->in = NULL;
    }

    if (numOut > 0) {
        db->data->out = (ChannelOut **) mcx_calloc(numOut, sizeof(ChannelOut *));
        if (db->data->out == NULL) {
            mcx_log(LOG_ERROR, "Ports: Memory allocation for outports failed");
            return RETURN_ERROR;
        }
        for (i = 0; i < numOut; i++) {
            db->data->out[i] = (ChannelOut *) object_create(ChannelOut);
            if (!db->data->out[i]) {
                mcx_log(LOG_ERROR, "Ports: Memory allocation for outport %d failed", i);
                // cleanup
                size_t j = 0;
                if (db->data->in) {
                    for (j = 0; j < DatabusInfoGetChannelNum(in); j++) {
                        object_destroy(db->data->in[j]);
                    }
                    mcx_free(db->data->in);
                    db->data->in = NULL;
                }
                for (j = 0; j < i; j++) {
                    object_destroy(db->data->out[j]);
                }
                mcx_free(db->data->out);
                db->data->out = NULL;
                return RETURN_ERROR;
            }
            db->data->out[i]->Setup(db->data->out[i], DatabusInfoGetChannel(out, i), config);
        }
    } else {
        db->data->out = NULL;
    }
    return RETURN_OK;
}


McxStatus DatabusTriggerOutChannels(Databus *db, TimeInterval * time) {
    if(!db) {
        mcx_log(LOG_ERROR, "Ports: Trigger outports: Invalid structure");
        return RETURN_ERROR;
    }
    Channel * out = NULL;
    size_t numOut = DatabusInfoGetChannelNum(DatabusGetOutInfo(db));
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < numOut; i++) {
        out = (Channel *) db->data->out[i];
        retVal = out->Update(out, time);
        if (RETURN_OK != retVal) {
            ChannelInfo * info = &out->info;
            mcx_log(LOG_ERROR, "Could not update outport %s", ChannelInfoGetName(info));
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}


McxStatus DatabusTriggerConnectedInConnections(Databus * db, TimeInterval * consumerTime) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Trigger inports: Invalid structure");
        return RETURN_ERROR;
    }
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < db->data->numInConnected; i++) {
        Channel * channel = (Channel *)db->data->inConnected[i];
        retVal = channel->Update(channel, consumerTime);
        if (RETURN_OK != retVal) {
            ChannelInfo * info = &channel->info;
            mcx_log(LOG_ERROR, "Could not update inport %s", ChannelInfoGetName(info));
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}


McxStatus DatabusTriggerInConnections(Databus * db, TimeInterval * consumerTime) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Trigger inports: Invalid structure");
        return RETURN_ERROR;
    }
    size_t numIn = DatabusInfoGetChannelNum(DatabusGetInInfo(db));
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < numIn; i++) {
        Channel * channel = (Channel *) db->data->in[i];
        if (channel->IsConnected(channel) || channel->info.defaultValue) {
            retVal = channel->Update(channel, consumerTime);
            if (RETURN_OK != retVal) {
                ChannelInfo * info = &channel->info;
                mcx_log(LOG_ERROR, "Could not update inport %s", ChannelInfoGetName(info));
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}


Connection * DatabusCreateConnection(Databus * db, ConnectionInfo * info) {
    if (!db || !info) {
        mcx_log(LOG_ERROR, "Ports: Create connection: Invalid structure");
        return NULL;
    }
    Databus * inDb = NULL;

    Component * target = NULL;

    Connection * connection = NULL;

    ChannelOut * outChannel = NULL;
    ChannelIn  * inChannel  = NULL;

    size_t outChannelID = info->sourceChannel;
    size_t inChannelID  = info->targetChannel;

    char * connStr = NULL;

    McxStatus retVal = RETURN_OK;

    // get outChannel
    if (outChannelID < 0 || outChannelID > DatabusInfoGetChannelNum(db->data->outInfo)) {
        mcx_log(LOG_ERROR, "Create connection: Illegal outport number %d", outChannelID);
        return NULL;
    }

    outChannel = db->data->out[outChannelID];

    target = info->targetComponent;

    // get inChannel
    inDb = target->GetDatabus(target);
    if (inChannelID < 0 || inChannelID > DatabusInfoGetChannelNum(DatabusGetInInfo(inDb))) {
        mcx_log(LOG_ERROR, "Create connection: Illegal inport number %d", inChannelID);
        return NULL;
    }

    inChannel = inDb->data->in[inChannelID];

    connStr = ConnectionInfoConnectionString(info);
    mcx_log(LOG_DEBUG, "  Connection: %s", connStr);
    if (connStr) {
        mcx_free(connStr);
    }

    {
        connection = (Connection *)object_create(FilteredConnection);
    }

    if (!connection) {
        return NULL;
    }

    retVal = connection->Setup(connection, outChannel, inChannel, info);
    if (RETURN_OK != retVal) {
        char * buffer = ConnectionInfoConnectionString(info);
        if (buffer) {
            mcx_log(LOG_ERROR, "Create connection: Could not setup connection %s", buffer);
            mcx_free(buffer);
        } else {
            mcx_log(LOG_ERROR, "Create connection: Could not setup connection");
        }
        return NULL;
    }

    return connection;
}

// Only returns NULL if db was NULL
DatabusInfo * DatabusGetInInfo(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get inport info: Invalid structure");
        return NULL;
    }

    return db->data->inInfo;
}

// Only returns NULL if db was NULL
DatabusInfo * DatabusGetOutInfo(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get outport info: Invalid structure");
        return NULL;
    }

    return db->data->outInfo;
}

// Only returns NULL if db was NULL
DatabusInfo * DatabusGetLocalInfo(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get internal port info: Invalid structure");
        return NULL;
    }

    return db->data->localInfo;
}

// Only returns NULL if db was NULL
DatabusInfo * DatabusGetRTFactorInfo(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get internal port info: Invalid structure");
        return NULL;
    }

    return db->data->rtfactorInfo;
}

// Only returns SIZE_T_ERROR if info was NULL
size_t DatabusInfoGetChannelNum(DatabusInfo * info) {
    if (!info) {
        mcx_log(LOG_ERROR, "Ports: Get port number: Invalid structure");
        return SIZE_T_ERROR;
    }

    return info->data->infos->Size(info->data->infos);
}

size_t DatabusInfoGetChannelElemNum(DatabusInfo * info) {
    size_t numInfos = info->data->infos->Size(info->data->infos);
    size_t i = 0;
    size_t numElems = 0;

    for (i = 0; i < numInfos; i++) {
        ChannelInfo * chInfo = (ChannelInfo*)info->data->infos->At(info->data->infos, i);
        if (chInfo->dimension) {
            // array
            numElems += ChannelDimensionNumElements(chInfo->dimension);
        } else {
            numElems += 1;
        }
    }

    return numElems;
}

ChannelInfo * DatabusInfoGetChannel(DatabusInfo * info, size_t i) {
    if (!info) {
        mcx_log(LOG_ERROR, "Ports: Get port info: Invalid structure");
        return NULL;
    }

    if (i >= info->data->infos->Size(info->data->infos)) {
        mcx_log(LOG_ERROR, "Ports: Get port info: Unknown port %d", i);
        return NULL;
    }

    return (ChannelInfo *) info->data->infos->At(info->data->infos, i);
}

int DatabusInChannelsDefined(Databus * db) {
    return db->data->in != NULL;
}

int DatabusOutChannelsDefined(Databus* db) {
    return db->data->out != NULL;
}

ChannelIn * DatabusGetInChannel(Databus * db, size_t i) {
    DatabusInfo * info = NULL;

    if (!db->data->in) {
        mcx_log(LOG_ERROR, "Ports: Get inport: No ports");
        return NULL;
    }

    info = DatabusGetInInfo(db);
    if (!info) {
        mcx_log(LOG_ERROR, "Ports: Get inport: No port info");
        return NULL;
    }
    if (i >= DatabusInfoGetChannelNum(info)) {
        mcx_log(LOG_ERROR, "Ports: Get inport: Unknown port %d", i);
        return NULL;
    }

    return db->data->in[i];
}

ChannelOut * DatabusGetOutChannel(Databus * db, size_t i) {
    DatabusInfo * info = NULL;

    if (!db->data->out) {
        mcx_log(LOG_ERROR, "Ports: Get outport: No ports");
        return NULL;
    }

    info = DatabusGetOutInfo(db);
    if (!info) {
        mcx_log(LOG_ERROR, "Ports: Get outport: No port info");
        return NULL;
    }
    if (i >= DatabusInfoGetChannelNum(info)) {
        mcx_log(LOG_ERROR, "Ports: Get outport: Unknown port %d", i);
        return NULL;
    }

    return db->data->out[i];
}

Channel * DatabusGetLocalChannel(Databus * db, size_t i) {
    DatabusInfo * info = NULL;

    if (!db->data->local) {
        mcx_log(LOG_ERROR, "Ports: Get local variable: No ports");
        return NULL;
    }

    info = db->data->localInfo;
    if (!info) {
        mcx_log(LOG_ERROR, "Ports: Get local variable: No port info");
        return NULL;
    }
    if (i >= DatabusInfoGetChannelNum(info)) {
        mcx_log(LOG_ERROR, "Ports: Get local variable: Unknown port %d", i);
        return NULL;
    }

    return (Channel *) db->data->local[i];
}

Channel * DatabusGetRTFactorChannel(Databus * db, size_t i) {
    DatabusInfo * info = NULL;

    if (!db->data->rtfactor) {
        mcx_log(LOG_ERROR, "Ports: Get rtfactor variable: No ports");
        return NULL;
    }

    info = db->data->rtfactorInfo;
    if (!info) {
        mcx_log(LOG_ERROR, "Ports: Get rtfactor variable: No port info");
        return NULL;
    }
    if (i >= DatabusInfoGetChannelNum(info)) {
        mcx_log(LOG_ERROR, "Ports: Get rtfactor variable: Unknown port %d", i);
        return NULL;
    }

    return (Channel *) db->data->rtfactor[i];
}

// Only returns SIZE_T_ERROR if db was NULL
size_t DatabusGetOutChannelsNum(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get outport number: Invalid structure");
        return SIZE_T_ERROR;
    }

    return DatabusInfoGetChannelNum(DatabusGetOutInfo(db));
}

// Only returns SIZE_T_ERROR if db was NULL
size_t DatabusGetInChannelsNum(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get inport number: Invalid structure");
        return SIZE_T_ERROR;
    }

    return DatabusInfoGetChannelNum(DatabusGetInInfo(db));
}

// Only returns SIZE_T_ERROR if db was NULL
size_t DatabusGetLocalChannelsNum(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get local variable number: Invalid structure");
        return SIZE_T_ERROR;
    }

    return DatabusInfoGetChannelNum(DatabusGetLocalInfo(db));
}

// Only returns SIZE_T_ERROR if db was NULL
size_t DatabusGetRTFactorChannelsNum(Databus * db) {
    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get rtfactor variable number: Invalid structure");
        return SIZE_T_ERROR;
    }

    return DatabusInfoGetChannelNum(DatabusGetRTFactorInfo(db));
}

size_t DatabusGetOutChannelsElemNum(Databus * db) {
    return DatabusInfoGetChannelElemNum(DatabusGetOutInfo(db));
}

size_t DatabusGetInChannelsElemNum(Databus * db) {
    return DatabusInfoGetChannelElemNum(DatabusGetInInfo(db));
}


McxStatus DatabusSetOutReference(Databus * db, size_t channel, const void * reference, ChannelType * type) {
    ChannelOut * out = NULL;

    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Set out reference: Invalid structure");
        return RETURN_ERROR;
    }
    if (!reference) {
        mcx_log(LOG_ERROR, "Ports: Set out reference: Invalid reference");
        return RETURN_ERROR;
    }

    out = DatabusGetOutChannel(db, channel);
    if (!out) {
        mcx_log(LOG_ERROR, "Ports: Set out reference: Unknown port %d", channel);
        return RETURN_ERROR;
    }

    if (ChannelTypeIsValid(type)) {
        ChannelInfo * info = &((Channel *)out)->info;
        if (!ChannelTypeEq(info->type, type)) {
            if (ChannelInfoIsBinary(info) && ChannelTypeIsBinary(type)) {
                // ok
            } else {
                mcx_log(LOG_ERROR, "Ports: Set out reference: Port %s has mismatching type %s, given: %s",
                        ChannelInfoGetName(info), ChannelTypeToString(info->type), ChannelTypeToString(type));
                return RETURN_ERROR;
            }
        }
    }

    return out->SetReference(out, reference, type);
}

McxStatus DatabusSetOutReferenceFunction(Databus * db, size_t channel, const void * reference, ChannelType *  type) {
    ChannelOut * out = NULL;
    ChannelInfo * info = NULL;

    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Set out reference function: Invalid structure");
        return RETURN_ERROR;
    }
    if (!reference) {
        mcx_log(LOG_ERROR, "Ports: Set out reference function: Invalid reference");
        return RETURN_ERROR;
    }

    out = DatabusGetOutChannel(db, channel);
    if (!out) {
        mcx_log(LOG_ERROR, "Ports: Set out reference function: Unknown port %d", channel);
        return RETURN_ERROR;
    }

    info = &((Channel *)out)->info;
    if (!ChannelTypeEq(info->type, type)) {
        mcx_log(LOG_ERROR, "Ports: Set out reference function: Port %s has mismatching type %s, given: %s",
            ChannelInfoGetName(info), ChannelTypeToString(info->type), ChannelTypeToString(type));
        return RETURN_ERROR;
    }
    if (ChannelInfoIsBinary(info)) {
        mcx_log(LOG_ERROR, "Ports: Set out reference function: Illegal type: Binary");
        return RETURN_ERROR;
    }

    return out->SetReferenceFunction(out, (const proc *) reference, type);
}

McxStatus DatabusSetInReference(Databus * db, size_t channel, void * reference, ChannelType *  type) {
    ChannelIn * in = NULL;
    ChannelInfo * info = NULL;

    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Set in-reference: Invalid structure");
        return RETURN_ERROR;
    }
    if (!reference) {
        mcx_log(LOG_ERROR, "Ports: Set in-reference: Invalid reference");
        return RETURN_ERROR;
    }

    in = DatabusGetInChannel(db, channel);
    if (!in) {
        mcx_log(LOG_ERROR, "Ports: Set in-reference: Unknown port %d", channel);
        return RETURN_ERROR;
    }

    if (ChannelTypeIsValid(type)) {
        info = &((Channel *)in)->info;
        if (!ChannelTypeEq(info->type, type)) {
            // TODO: Remove ChannelInfoIsBinary, use ChannelTypeIsBinary instead?
            if (ChannelInfoIsBinary(info) && ChannelTypeIsBinary(type)) {
                // ok
            } else {
                mcx_log(LOG_ERROR, "Ports: Set in-reference: Port %s has mismatching type %s, given: %s",
                        ChannelInfoGetName(info), ChannelTypeToString(info->type), ChannelTypeToString(type));
                return RETURN_ERROR;
            }
        }
    }

    return in->SetReference(in, reference, type);
}


static char * DatabusGetUniqueChannelName(Databus * db, const char * name) {
#define SUFFIX_LEN 5
    char * uniqueName   = NULL;
    size_t suffix       = 1;
    char   suffixStr[SUFFIX_LEN] = "";

    Vector * inInfos = db->data->inInfo->data->infos;
    Vector * outInfos = db->data->outInfo->data->infos;
    Vector * localInfos = db->data->localInfo->data->infos;
    Vector * rtfactorInfos = db->data->rtfactorInfo->data->infos;

    /* Make name unique by adding " %d" suffix */
    uniqueName = (char *) mcx_calloc(strlen(name) + SUFFIX_LEN + 1, sizeof(char));
    strcpy(uniqueName, name);
    strcat(uniqueName, suffixStr);

    while (ChannelInfosGetNameIdx(inInfos, uniqueName) > -1
           || ChannelInfosGetNameIdx(outInfos, uniqueName) > -1
           || ChannelInfosGetNameIdx(localInfos, uniqueName) > -1) {
        int len = snprintf(suffixStr, SUFFIX_LEN," %zu", suffix);
        strcpy(uniqueName, name);
        strcat(uniqueName, suffixStr);
        suffix++;
    }

    return uniqueName;
}

static McxStatus DatabusAddLocalChannelInternal(Databus * db,
                                                ChannelLocal * * * dbDataChannel,
                                                DatabusInfoData * infoData,
                                                const char * name,
                                                const char * id,
                                                const char * unit,
                                                const void * reference,
                                                ChannelType * type) {
    ChannelInfo chInfo = { 0 };
    ChannelLocal * local = NULL;
    Channel * channel = NULL;
    size_t infoDataSize = 0;
    ChannelDimension * dimension = NULL;

    char * uniqueName = NULL;

    McxStatus retVal = RETURN_OK;


    retVal = ChannelInfoInit(&chInfo);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Create port info for %s failed", name);
        return RETURN_ERROR;
    }

    uniqueName = DatabusGetUniqueChannelName(db, name);

    switch (ChannelTypeNumDims(type)) {
        case 0:
            dimension = NULL;
            break;
        case 1:
            dimension = ChannelDimensionCreate1D(0, ChannelTypeNumElements(type) - 1);
            break;
        default:
            mcx_log(LOG_ERROR, "Ports: Set local-reference: Only scalars or 1-dimensional arrays supported (encountered %d for %s)", ChannelTypeNumDims(type), uniqueName);
            goto cleanup;
    }
    retVal = ChannelInfoSetup(&chInfo, uniqueName, uniqueName, NULL, unit, type, id, dimension);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Setting up ChannelInfo for %s failed", ChannelInfoGetName(&chInfo));
        goto cleanup;
    }
    mcx_free(uniqueName);

    retVal = infoData->infos->PushBack(infoData->infos, &chInfo);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Storing ChannelInfo for %s failed", ChannelInfoGetName(&chInfo));
        goto cleanup;
    }

    infoDataSize = infoData->infos->Size(infoData->infos);

    local = (ChannelLocal *) object_create(ChannelLocal);
    if (!local) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Create port %s failed", ChannelInfoGetName(&chInfo));
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    channel = (Channel *) local;
    retVal = channel->Setup(channel, infoData->infos->At(infoData->infos, infoDataSize - 1));
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Could not setup port %s", ChannelInfoGetName(&chInfo));
        goto cleanup;
    }

    retVal = local->SetReference(local, reference, type);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Setting reference to %s failed", ChannelInfoGetName(&chInfo));
        goto cleanup;
    }

    *dbDataChannel = (ChannelLocal * *) mcx_realloc(*dbDataChannel, infoDataSize * sizeof(Channel *));
    if (!*dbDataChannel) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Memory reallocation for adding %s to ports failed", ChannelInfoGetName(&chInfo));
        retVal = RETURN_ERROR;
        goto cleanup;
    }
    (*dbDataChannel)[infoDataSize - 1] = (ChannelLocal *) channel;

cleanup:
    ChannelInfoDestroy(&chInfo);

    return retVal;
}

McxStatus DatabusAddLocalChannel(Databus * db,
                                 const char * name,
                                 const char * id,
                                 const char * unit,
                                 const void * reference,
                                 ChannelType * type) {
    DatabusData * dbData = db->data;
    DatabusInfoData * infoData = dbData->localInfo->data;

    McxStatus retVal = RETURN_OK;

    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Invalid structure");
        return RETURN_ERROR;
    }
    if (!reference) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Invalid reference");
        return RETURN_ERROR;
    }

    retVal = DatabusAddLocalChannelInternal(db, &dbData->local, infoData, name, id, unit, reference, type);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Ports: Set local-reference: Setup failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus DatabusAddRTFactorChannel(Databus * db,
                                    const char * name,
                                    const char * id,
                                    const char * unit,
                                    const void * reference,
                                    ChannelType * type) {
    DatabusData * dbData = db->data;
    DatabusInfoData * infoData = dbData->rtfactorInfo->data;

    McxStatus retVal = RETURN_OK;

    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Set RTFactor-reference: Invalid structure");
        return RETURN_ERROR;
    }
    if (!reference) {
        mcx_log(LOG_ERROR, "Ports: Set RTFactor-reference: Invalid reference");
        return RETURN_ERROR;
    }

    retVal = DatabusAddLocalChannelInternal(db, &dbData->rtfactor, infoData, name, id, unit, reference, type);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Ports: Set RTFactor-reference: Setup failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

const void * DatabusGetInValueReference(Databus * db, size_t channel) {
    ChannelIn * in = NULL;

    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get in-reference: Invalid structure");
        return NULL;
    }

    in = DatabusGetInChannel(db, channel);
    if (!in) {
        mcx_log(LOG_ERROR, "Ports: Get in-reference: Unknown port", channel);
        return NULL;
    }

    return ((Channel *)in)->GetValueReference((Channel *)in);
}

const void * DatabusGetOutValueReference(Databus * db, size_t channel) {
    ChannelOut * out = NULL;

    if (!db) {
        mcx_log(LOG_ERROR, "Ports: Get out-reference: Invalid structure");
        return NULL;
    }

    out = DatabusGetOutChannel(db, channel);
    if (!out) {
        mcx_log(LOG_ERROR, "Ports: Get out-reference: Unknown port", channel);
        return NULL;
    }

    return ((Channel *)out)->GetValueReference((Channel *)out);
}


ChannelInfo * DatabusGetInChannelInfo(Databus * db, size_t channel) {
    DatabusInfoData * data = db->data->inInfo->data;

    if (channel >= data->infos->Size(data->infos)) {
        mcx_log(LOG_ERROR, "Ports: Get in-info: Unknown port %d", channel);
        return NULL;
    }

    return (ChannelInfo *) data->infos->At(data->infos, channel);
}

ChannelInfo * DatabusGetOutChannelInfo(Databus * db, size_t channel) {
    DatabusInfoData * data = data = db->data->outInfo->data;

    if (channel >= data->infos->Size(data->infos)) {
        mcx_log(LOG_ERROR, "Ports: Get out-info: Unknown port %d", channel);
        return NULL;
    }

    return (ChannelInfo *) data->infos->At(data->infos, channel);
}

ChannelInfo * DatabusGetLocalChannelInfo(Databus * db, size_t channel) {
    DatabusInfoData * data = db->data->localInfo->data;

    if (channel >= data->infos->Size(data->infos)) {
        mcx_log(LOG_ERROR, "Ports: Get local-info: Unknown port %d", channel);
        return NULL;
    }

    return (ChannelInfo *) data->infos->At(data->infos, channel);
}

McxStatus DatabusCollectModeSwitchData(Databus * db) {
    Vector * infos = db->data->outInfo->data->infos;
    size_t size = infos->Size(infos);
    size_t i = 0, j = 0, idx = 0;
    db->modeSwitchDataSize = 0;

    // determine cache size
    for (i = 0; i < size; i++) {
        ChannelOut * out = db->data->out[i];
        ConnectionList * conns = out->GetConnections(out);
        db->modeSwitchDataSize += conns->numConnections;
    }

    // allocate cache
    db->modeSwitchData = (ModeSwitchData *)mcx_calloc(db->modeSwitchDataSize, sizeof(ModeSwitchData));
    if (!db->modeSwitchData) {
        return RETURN_ERROR;
    }

    // fill up the cache
    for (i = 0, idx = 0; i < size; i++) {
        ChannelOut * out = db->data->out[i];
        ConnectionList * conns = out->GetConnections(out);
        size_t connSize = conns->numConnections;

        for (j = 0; j < connSize; j++, idx++) {
            Connection * connection = conns->connections[j];
            ConnectionInfo * info = connection->GetInfo(connection);
            Component * target = info->targetComponent;
            Component * source = info->sourceComponent;
            double targetTimeStepSize = target->GetTimeStep(target);
            double sourceTimeStepSize = source->GetTimeStep(source);

            db->modeSwitchData[idx].connection = connection;
            db->modeSwitchData[idx].sourceTimeStepSize = sourceTimeStepSize;
            db->modeSwitchData[idx].targetTimeStepSize = targetTimeStepSize;
        }
    }

    return RETURN_OK;
}

McxStatus DatabusEnterCouplingStepMode(Databus * db, double timeStepSize) {
    size_t i = 0;
    McxStatus retVal = RETURN_OK;

    for (i = 0; i < db->modeSwitchDataSize; i++) {
        ModeSwitchData data = db->modeSwitchData[i];
        retVal = data.connection->EnterCouplingStepMode(data.connection, timeStepSize, data.sourceTimeStepSize, data.targetTimeStepSize);
        if (RETURN_OK != retVal) {
            ConnectionInfo * info = data.connection->GetInfo(data.connection);
            char * buffer = ConnectionInfoConnectionString(info);
            mcx_log(LOG_ERROR, "Ports: Cannot enter coupling step mode of connection %s", buffer);
            mcx_free(buffer);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus DatabusEnterCommunicationMode(Databus * db, double time) {
    size_t i = 0;
    McxStatus retVal = RETURN_OK;

    for (i = 0; i < db->modeSwitchDataSize; i++) {
        ModeSwitchData data = db->modeSwitchData[i];
        retVal = data.connection->EnterCommunicationMode(data.connection, time);
        if (RETURN_OK != retVal) {
            ConnectionInfo * info = data.connection->GetInfo(data.connection);
            char * buffer = ConnectionInfoConnectionString(info);
            mcx_log(LOG_ERROR, "Ports: Cannot enter communication mode of connection %s", buffer);
            mcx_free(buffer);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

/* The container connections may only contain connections outgoing from this db */
McxStatus DatabusEnterCommunicationModeForConnections(Databus * db, ObjectList * connections, double time) {
    size_t i = 0;

    size_t connSize = connections->Size(connections);

    for (i = 0; i < connSize; i++) {
        Connection * connection = (Connection *) connections->At(connections, i);
        ConnectionInfo * info = connection->GetInfo(connection);
        Component * comp = info->sourceComponent;
        Databus * connDb = comp->GetDatabus(comp);

        McxStatus retVal = RETURN_OK;

        if (db == connDb) {
            retVal = connection->EnterCommunicationMode(connection, time);
            if (RETURN_OK != retVal) {
                char * buffer = ConnectionInfoConnectionString(info);
                mcx_log(LOG_ERROR, "Ports: Cannot enter communication mode of connection %s", buffer);
                mcx_free(buffer);
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static void DatabusDestructor(Databus * db) {
    object_destroy(db->data);
}

static Databus * DatabusCreate(Databus * db) {
    db->data = (DatabusData *) object_create(DatabusData);
    if (!db->data) {
        return NULL;
    }

    return db;
}

OBJECT_CLASS(Databus, Object);


char * CreateChannelID(const char * compName, const char * channelName) {
    const char separator = '#';
    size_t len = strlen(compName) + strlen(channelName) + 2;
    char * id = (char *) mcx_calloc(len, sizeof(char));
    if (id) {
        snprintf(id, len, "%s%c%s", compName, separator, channelName);
    }
    return id;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */