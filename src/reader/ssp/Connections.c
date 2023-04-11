/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Connections.h"

#include "reader/EnumMapping.h"
#include "reader/ssp/Schema.h"
#include "reader/model/connections/ConnectionInput.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static DecoupleIfNeededInput * SSDReadDecoupleIfNeededInput(xmlNodePtr ifNeededNode) {
    DecoupleIfNeededInput * input = (DecoupleIfNeededInput *)object_create(DecoupleIfNeededInput);
    InputElement * element = (InputElement *)input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *)ifNeededNode;

    retVal = xml_opt_attr_int32(ifNeededNode, "priority", &input->priority);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(input);
        return NULL;
    }

    return input;
}


static McxStatus SSDReadCouplingSettings(ConnectionInput * connectionInput, xmlNodePtr connectionNode) {
    xmlNodePtr annotationNode = NULL;
    xmlNodePtr decouplingNode = NULL;

    McxStatus retVal = RETURN_OK;

    annotationNode = xml_annotation_of_type(connectionNode, "com.avl.model.connect.ssp.connection.decoupling");
    if (!annotationNode) {
        OPTIONAL_UNSET(connectionInput->decoupleType);
        return RETURN_OK;
    }

    decouplingNode = xml_child(annotationNode, "Decoupling");
    if (!decouplingNode) {
        return xml_error_missing_child(annotationNode, "Decoupling");
    }

    retVal = xml_validate_node(decouplingNode, "com.avl.model.connect.ssp.connection.decoupling");
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    {
        xmlNodePtr neverNode = xml_child(decouplingNode, "Never");
        xmlNodePtr alwaysNode = xml_child(decouplingNode, "Always");
        xmlNodePtr ifNeededNode = xml_child(decouplingNode, "IfNeeded");

        if (neverNode) {
            OPTIONAL_SET(connectionInput->decoupleType, DECOUPLE_NEVER);
        } else if (alwaysNode) {
            OPTIONAL_SET(connectionInput->decoupleType, DECOUPLE_ALWAYS);
        } else if (ifNeededNode) {
            connectionInput->decoupleSettings = (InputElement*)SSDReadDecoupleIfNeededInput(ifNeededNode);
            if (!connectionInput->decoupleSettings) {
                return RETURN_ERROR;
            }

            OPTIONAL_SET(connectionInput->decoupleType, DECOUPLE_IFNEEDED);
        } else {
            return xml_error_generic(decouplingNode, "Unknown coupling settings");
        }
    }

    return RETURN_OK;
}

static InterExtrapolationInput * SSDReadPolynomialInterExtrapolation(xmlNodePtr interExtrapolationNode) {
    InterExtrapolationInput * input = (InterExtrapolationInput *)object_create(InterExtrapolationInput);
    InputElement * element = (InputElement *)input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *)interExtrapolationNode;


    retVal = xml_attr_enum(interExtrapolationNode, "extrapolationInterval", interExtrapolationIntervalMapping, (int*)&input->extrapolationType, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_attr_enum(interExtrapolationNode, "interpolationInterval", interExtrapolationIntervalMapping, (int*)&input->interpolationType, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_attr_enum(interExtrapolationNode, "extrapolationOrder", interExtrapolationOrderMapping, (int*)&input->extrapolationOrder, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_attr_enum(interExtrapolationNode, "interpolationOrder", interExtrapolationOrderMapping, (int*)&input->interpolationOrder, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(input);
        return NULL;
    }

    return input;
}


static McxStatus SSDReadInterExtrapolationInput(ConnectionInput * connectionInput, xmlNodePtr connectionNode) {
    xmlNodePtr annotationNode = NULL;
    xmlNodePtr interExtrapolationNode = NULL;

    McxStatus retVal = RETURN_OK;

    annotationNode = xml_annotation_of_type(connectionNode, "com.avl.model.connect.ssp.connection.inter_extrapolation");
    if (!annotationNode) {
        OPTIONAL_UNSET(connectionInput->interExtrapolationType);
        return RETURN_OK;
    }

    interExtrapolationNode = xml_child(annotationNode, "InterExtrapolation");
    if (!interExtrapolationNode) {
        return xml_error_missing_child(annotationNode, "InterExtrapolation");
    }

    retVal = xml_validate_node(interExtrapolationNode, "com.avl.model.connect.ssp.connection.inter_extrapolation");
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    connectionInput->interExtrapolation = SSDReadPolynomialInterExtrapolation(interExtrapolationNode);
    if (!connectionInput->interExtrapolation) {
        return RETURN_ERROR;
    }

    OPTIONAL_SET(connectionInput->interExtrapolationType, INTEREXTRAPOLATION_POLYNOMIAL);

    return RETURN_OK;
}


static McxStatus SSDConnectionConfigurationSupported(xmlNodePtr connectionNode) {
    int32_t suppressUnitConversion = FALSE;
    xmlNodePtr linTransformNode = NULL;
    xmlNodePtr intTransformNode = NULL;
    xmlNodePtr boolTransformNode = NULL;
    xmlNodePtr enumTransformNode = NULL;

    if (xml_attr_bool32(connectionNode, "suppressUnitConversion", &suppressUnitConversion, SSD_OPTIONAL) == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    if (suppressUnitConversion) {
        return xml_error_generic(connectionNode, "Unit conversion suppresion is not supported");
    }

    linTransformNode = xml_child(connectionNode, "LinearTransformation");
    if (linTransformNode) {
        return xml_error_unsupported_node(linTransformNode);
    }

    boolTransformNode = xml_child(connectionNode, "BooleanMappingTransformation");
    if (boolTransformNode) {
        return xml_error_unsupported_node(linTransformNode);
    }

    intTransformNode = xml_child(connectionNode, "IntegerMappingTransformation");
    if (intTransformNode) {
        return xml_error_unsupported_node(intTransformNode);
    }

    enumTransformNode = xml_child(connectionNode, "EnumerationMappingTransformation");
    if (enumTransformNode) {
        return xml_error_unsupported_node(enumTransformNode);
    }

    return RETURN_OK;
}


typedef struct {
    const char * element;
    const char * connector;
    const char * extension;
} EndpointDesc;


static EndpointDesc startEndpoint = { "startElement", "startConnector", "Start" };
static EndpointDesc endEndpoint = { "endElement", "endConnector", "End" };


static int SSDEndpointIsVector(EndpointDesc * desc, xmlNodePtr connectionNode) {
    xmlNodePtr annotationNode = NULL;
    xmlNodePtr mcConnectionNode = NULL;

    annotationNode = xml_annotation_of_type(connectionNode, "com.avl.model.connect.ssp.connection");
    if (!annotationNode) {
        return FALSE;
    }

    mcConnectionNode = xml_child(annotationNode, "Connection");
    if (!mcConnectionNode) {
        return FALSE;
    }

    if (!xml_child(mcConnectionNode, desc->extension)) {
        return FALSE;
    }

    return TRUE;
}


static ScalarEndpointInput * SSDReadScalarEndpoint(xmlNodePtr connectionNode, EndpointDesc * desc) {
    ScalarEndpointInput * input = (ScalarEndpointInput*)object_create(ScalarEndpointInput);
    InputElement * element = (InputElement *) input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) connectionNode;

    retVal = xml_attr_string(connectionNode, desc->element, &input->component, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_attr_string(connectionNode, desc->connector, &input->channel, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    {
        // check that no indices are defined
        xmlNodePtr annotationNode = xml_annotation_of_type(connectionNode, "com.avl.model.connect.ssp.connection");
        if (annotationNode) {
            xmlNodePtr mcConnectionNode = xml_child(annotationNode, "Connection");
            if (mcConnectionNode) {
                xmlNodePtr endpointNode = xml_child(mcConnectionNode, desc->extension);
                if (endpointNode) {
                    retVal = xml_error_generic(endpointNode, "Definition of indices is not allowed for scalar connectors");
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(input);
        return NULL;
    }

    return input;
}


static VectorEndpointInput * SSDReadVectorEndpoint(xmlNodePtr connectionNode, EndpointDesc * desc) {
    VectorEndpointInput * input = (VectorEndpointInput*)object_create(VectorEndpointInput);
    InputElement * element = (InputElement *) input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) connectionNode;

    retVal = xml_attr_string(connectionNode, desc->element, &input->component, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_attr_string(connectionNode, desc->connector, &input->channel, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    {
        xmlNodePtr annotationNode = xml_annotation_of_type(connectionNode, "com.avl.model.connect.ssp.connection");
        xmlNodePtr mcConnectionNode = NULL;
        xmlNodePtr endpointNode = NULL;

        mcConnectionNode = xml_child(annotationNode, "Connection");
        endpointNode = xml_child(mcConnectionNode, desc->extension);
        if (!endpointNode) {
            retVal = xml_error_missing_child(mcConnectionNode, desc->extension);
            goto cleanup;
        }

        retVal = xml_attr_int64(endpointNode, "startIndex", &input->startIndex, SSD_MANDATORY);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        retVal = xml_attr_int64(endpointNode, "endIndex", &input->endIndex, SSD_MANDATORY);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(input);
        return NULL;
    }

    return input;
}


static ConnectionInput * SSDReadConnection(xmlNodePtr connectionNode) {
    ConnectionInput * connectionInput = (ConnectionInput *) object_create(ConnectionInput);
    InputElement * element = (InputElement *) connectionInput;

    McxStatus retVal = RETURN_OK;

    if (!connectionInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) connectionNode;

    {
        // validate the connection annotation if present
        xmlNodePtr annotationNode = xml_annotation_of_type(connectionNode, "com.avl.model.connect.ssp.connection");
        if (annotationNode) {
            xmlNodePtr mcConnectionNode = xml_child(annotationNode, "Connection");
            if (!mcConnectionNode) {
                retVal = xml_error_missing_child(annotationNode, "Connection");
                goto cleanup;
            }

            retVal = xml_validate_node(mcConnectionNode, "com.avl.model.connect.ssp.connection");
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }
        }
    }

    {
        // check if the connection is supported
        if (SSDConnectionConfigurationSupported(connectionNode) == RETURN_ERROR) {
            retVal = RETURN_ERROR;
            goto cleanup;
        }
    }

    {
        // read start endpoint
        if (SSDEndpointIsVector(&startEndpoint, connectionNode)) {
            connectionInput->fromType = ENDPOINT_VECTOR;
            connectionInput->from.vectorEndpoint = SSDReadVectorEndpoint(connectionNode, &startEndpoint);
            if (!connectionInput->from.vectorEndpoint) {
                goto cleanup;
            }
        } else {
            connectionInput->fromType = ENDPOINT_SCALAR;
            connectionInput->from.scalarEndpoint = SSDReadScalarEndpoint(connectionNode, &startEndpoint);
            if (!connectionInput->from.scalarEndpoint) {
                goto cleanup;
            }
        }

        // read end endpoint
        if (SSDEndpointIsVector(&endEndpoint, connectionNode)) {
            connectionInput->toType = ENDPOINT_VECTOR;
            connectionInput->to.vectorEndpoint = SSDReadVectorEndpoint(connectionNode, &endEndpoint);
            if (!connectionInput->to.vectorEndpoint) {
                goto cleanup;
            }
        } else {
            connectionInput->toType = ENDPOINT_SCALAR;
            connectionInput->to.scalarEndpoint = SSDReadScalarEndpoint(connectionNode, &endEndpoint);
            if (!connectionInput->to.scalarEndpoint) {
                goto cleanup;
            }
        }
    }

    retVal = SSDReadInterExtrapolationInput(connectionInput, connectionNode);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = SSDReadCouplingSettings(connectionInput, connectionNode);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(connectionInput);
        return NULL;
    }

    return connectionInput;
}


ConnectionsInput * SSDReadConnections(xmlNodePtr connectionsNode) {
    ConnectionsInput * connectionsInput = (ConnectionsInput*)object_create(ConnectionsInput);
    InputElement * element = (InputElement *) connectionsInput;
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    if (!connectionsInput) {
        retVal = RETURN_ERROR;
        goto cleanup_0;
    }

    element->type = INPUT_SSD;
    element->context = (void *) connectionsNode;

    for (i = 0; i < xml_num_children(connectionsNode); i++) {
        xmlNodePtr connectionNode = xml_child_by_index(connectionsNode, i);
        ConnectionInput * connection = SSDReadConnection(connectionNode);

        if (!connection) {
            retVal = RETURN_ERROR;
            goto cleanup_1;
        }

        retVal = connectionsInput->connections->PushBack(connectionsInput->connections, (Object *) connection);
        if (RETURN_ERROR == retVal) {
            goto cleanup_1;
        }

        continue;

    cleanup_1:
        object_destroy(connection);
        goto cleanup_0;
    }

cleanup_0:
    if (RETURN_ERROR == retVal) {
        object_destroy(connectionsInput);
    }

    return connectionsInput;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */