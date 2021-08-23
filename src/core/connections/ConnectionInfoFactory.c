/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/connections/ConnectionInfoFactory.h"
#include "core/connections/ConnectionInfo.h"
#include "core/channels/ChannelDimension.h"
#include "core/Databus.h"
#include "objects/Vector.h"

#include "util/stdlib.h"
#include "util/string.h"

#include "core/channels/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus ConnectionInfoFactoryInitConnectionInfo(ConnectionInfo * info,
                                                         ObjectContainer * components,
                                                         ConnectionInput * connInput,
                                                         Component * sourceCompOverride,
                                                         Component * targetCompOverride) {
    McxStatus retVal = RETURN_OK;

    int id = 0;
    int connectionInverted = 0;
    char * strToChannel = NULL;
    char * strFromChannel = NULL;

    retVal = ConnectionInfoInit(info);
    if (retVal == RETURN_ERROR) {
        mcx_log(LOG_ERROR, "Initializing the ConnectionInfo object failed");
        goto cleanup;
    }

    // source component
    {
        // use conn endpoints from caller if they are not in components
        info->sourceComponent = sourceCompOverride;

        if (info->sourceComponent == NULL) {
            char * inputFromComponent = connInput->fromType == ENDPOINT_SCALAR ? connInput->from.scalarEndpoint->component :
                                                                                 connInput->from.vectorEndpoint->component;
            if (inputFromComponent == NULL) {
                retVal = input_element_error((InputElement*)connInput, "Source element is not specified");
                goto cleanup;
            }

            if (0 == strlen(inputFromComponent)) {
                retVal = input_element_error((InputElement*)connInput, "Source element name is empty");
                goto cleanup;
            }

            id = components->GetNameIndex(components, inputFromComponent);
            if (id < 0) {
                retVal = input_element_error((InputElement*)connInput, "Source element %s does not exist",
                                             inputFromComponent);
                goto cleanup;
            }

            info->sourceComponent = (Component *) components->elements[id];
        }
    }

    // target component
    {
        // use conn endpoints from caller if they are not in components
        info->targetComponent = targetCompOverride;

        if (info->targetComponent == NULL) {
            char * inputToComponent = connInput->toType == ENDPOINT_SCALAR ? connInput->to.scalarEndpoint->component :
                                                                             connInput->to.vectorEndpoint->component;
            if (inputToComponent == NULL) {
                retVal = input_element_error((InputElement*)connInput, "Target element is not specified");
                goto cleanup;
            }

            if (0 == strlen(inputToComponent)) {
                retVal = input_element_error((InputElement*)connInput, "Target element name is empty");
                goto cleanup;
            }

            id = components->GetNameIndex(components, inputToComponent);
            if (id < 0) {
                retVal = input_element_error((InputElement*)connInput, "Target element %s does not exist",
                                             inputToComponent);
                goto cleanup;
            }

            info->targetComponent = (Component *) components->elements[id];
        }
    }

    // source channel
    {
        Databus * databus = info->sourceComponent->GetDatabus(info->sourceComponent);
        DatabusInfo * databusInfo = DatabusGetOutInfo(databus);

        char * inputFromChannel = connInput->fromType == ENDPOINT_SCALAR ? connInput->from.scalarEndpoint->channel :
                                                                           connInput->from.vectorEndpoint->channel;

        strFromChannel = mcx_string_copy(inputFromChannel);
        if (0 == strlen(strFromChannel)) {
            retVal = input_element_error((InputElement*)connInput, "Source port name is empty");
            goto cleanup;
        }

        // TODO: multiplexing here?
        if (connInput->fromType == ENDPOINT_VECTOR) {
            ChannelDimension * sourceDimension = (ChannelDimension *) object_create(ChannelDimension);
            if (!sourceDimension) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            if (RETURN_OK != ChannelDimensionSetup(sourceDimension, 1)) {
                mcx_log(LOG_ERROR, "Could not setup ChannelDimension");
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            if (RETURN_OK != ChannelDimensionSetDimension(sourceDimension, 0, (size_t) connInput->from.vectorEndpoint->startIndex, (size_t) connInput->from.vectorEndpoint->endIndex)) {
                mcx_log(LOG_ERROR, "Could not SetDimension");
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            info->sourceDimension = sourceDimension;
        }

        info->sourceChannel = DatabusInfoGetChannelID(databusInfo, strFromChannel);
        if (info->sourceChannel < 0) {
            // the connection might be inverted, see SSP 1.0 specification (section 5.3.2.1, page 47)

            databusInfo = DatabusGetInInfo(databus);
            info->sourceChannel = DatabusInfoGetChannelID(databusInfo, strFromChannel);

            if (info->sourceChannel < 0) {
                mcx_log(LOG_ERROR, "Connection: Source port %s of element %s does not exist",
                    strFromChannel, info->sourceComponent->GetName(info->sourceComponent));
                retVal = RETURN_ERROR;
                goto cleanup;
            } else {
                connectionInverted = 1;
            }
        }
    }

    // target channel
    {
        Databus * databus = info->targetComponent->GetDatabus(info->targetComponent);
        DatabusInfo * databusInfo = NULL;

        char * inputToChannel = connInput->toType == ENDPOINT_SCALAR ? connInput->to.scalarEndpoint->channel :
                                                                       connInput->to.vectorEndpoint->channel;
        strToChannel = mcx_string_copy(inputToChannel);
        if (0 == strlen(strToChannel)) {
            retVal = input_element_error((InputElement*)connInput, "Target port name is empty");
            goto cleanup;
        }

        if (connInput->toType == ENDPOINT_VECTOR) {
            mcx_free(strToChannel);
            strToChannel = CreateIndexedName(inputToChannel, connInput->to.vectorEndpoint->startIndex);
            if (!strToChannel) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }
        }

        if (0 == connectionInverted) {
            databusInfo = DatabusGetInInfo(databus);
        } else {
            databusInfo = DatabusGetOutInfo(databus);
        }

        info->targetChannel = DatabusInfoGetChannelID(databusInfo, strToChannel);
        if (info->targetChannel < 0) {
            if (0 == connectionInverted) {
                mcx_log(LOG_ERROR, "Connection: Target port %s of element %s does not exist",
                    strToChannel, info->targetComponent->GetName(info->targetComponent));
            } else {
                mcx_log(LOG_ERROR, "Connection: Source port %s of element %s does not exist",
                    strToChannel, info->targetComponent->GetName(info->targetComponent));
            }
            retVal = RETURN_ERROR;
            goto cleanup;
        }
    }

    // swap endpoints if connection is inverted
    if (connectionInverted) {
        int tmp = info->sourceChannel;
        Component * tmpCmp = info->sourceComponent;

        info->sourceChannel = info->targetChannel;
        info->targetChannel = tmp;

        info->sourceComponent = info->targetComponent;
        info->targetComponent = tmpCmp;

        mcx_log(LOG_DEBUG, "Connection: Inverted connection (%s, %s) -- (%s, %s)",
            info->targetComponent->GetName(info->targetComponent), strFromChannel, info->sourceComponent->GetName(info->sourceComponent), strToChannel);
    }

    // extrapolation
    if (connInput->interExtrapolationType.defined) {
        info->interExtrapolationType = connInput->interExtrapolationType.value;
    }

    if (INTEREXTRAPOLATION_POLYNOMIAL == info->interExtrapolationType) {
        // read the parameters of poly inter-/extrapolation
        InterExtrapolationParams * params = &info->interExtrapolationParams;
        InterExtrapolationInput * paramsInput = connInput->interExtrapolation;

        params->extrapolationInterval = paramsInput->extrapolationType;
        params->interpolationInterval = paramsInput->interpolationType;
        params->interpolationOrder = paramsInput->interpolationOrder;
        params->extrapolationOrder = paramsInput->extrapolationOrder;
    }

    // decouple
    if (connInput->decoupleType.defined) {
        info->decoupleType = connInput->decoupleType.value;

        // decouple priority
        if (DECOUPLE_IFNEEDED == info->decoupleType) {
            DecoupleIfNeededInput * decoupleInput = (DecoupleIfNeededInput*)connInput->decoupleSettings;
            if (decoupleInput->priority.defined) {
                info->decouplePriority = decoupleInput->priority.value;
            }

            if (info->decouplePriority < 0) {
                retVal = input_element_error((InputElement*)decoupleInput, "Invalid decouple priority");
                goto cleanup;
            }
        }
    }

cleanup:
    if (NULL != strFromChannel) { mcx_free(strFromChannel); }
    if (NULL != strToChannel) { mcx_free(strToChannel); }

    return retVal;
}

Vector * ConnectionInfoFactoryCreateConnectionInfos(
    ObjectContainer * components,
    ConnectionInput * connInput,
    Component * sourceCompOverride,
    Component * targetCompOverride)
{
    McxStatus retVal = RETURN_OK;

    ConnectionInfo info = { 0 };
    Vector * list = NULL;

    list = (Vector *) object_create(Vector);
    if (!list) {
        goto cleanup;
    }

    list->Setup(list, sizeof(ConnectionInfo), ConnectionInfoInit, NULL, NULL);

    retVal = ConnectionInfoFactoryInitConnectionInfo(&info, components, connInput, sourceCompOverride, targetCompOverride);
    if (RETURN_ERROR == retVal) {
        goto cleanup;
    }

    /* info is the only connection: leave as is */
    list->PushBack(list, &info);

    return list;

cleanup:
    if (list) {
        object_destroy(list);
    }

    return NULL;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */