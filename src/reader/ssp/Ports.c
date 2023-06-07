/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Ports.h"

#include "reader/ssp/Schema.h"
#include "reader/ssp/Units.h"

#include "reader/model/ports/PortsInput.h"
#include "reader/model/ports/ScalarPortInput.h"
#include "reader/model/ports/VectorPortInput.h"

#include "core/channels/ChannelValue.h"
#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static MapStringChannelType _typeMapping[] = {
    {   "Real",  &ChannelTypeDouble},
    {"Integer", &ChannelTypeInteger},
    {"Boolean",    &ChannelTypeBool},
    { "String",  &ChannelTypeString},
    { "Binary",  &ChannelTypeBinary},
    {     NULL, &ChannelTypeUnknown},
};

static MapStringChannelType _vectorTypeMapping[] = {
    {"RealVector",    &ChannelTypeDouble},
    {"IntegerVector", &ChannelTypeInteger},
    {"BooleanVector", &ChannelTypeBool},
    {NULL,            &ChannelTypeUnknown},
};


int SSDConnectorIsVectorPort(xmlNodePtr connectorNode) {
    xmlNodePtr annotationNode = xml_annotation_of_type(connectorNode, "com.avl.model.connect.ssp.port");

    if (annotationNode) {
        xmlNodePtr portNode = xml_child(annotationNode, "Port");

        if (xml_child(portNode, "RealVector") || xml_child(portNode, "IntegerVector")) {
            return TRUE;
        }
    }
    return FALSE;
}

VectorPortInput * SSDReadComponentVectorPort(xmlNodePtr connectorNode, const char * type, ObjectContainer * units) {
    VectorPortInput * vectorPortInput = (VectorPortInput *) object_create(VectorPortInput);
    InputElement * element = (InputElement *) vectorPortInput;

    McxStatus retVal = RETURN_OK;

    if (!vectorPortInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) connectorNode;

    {
        retVal = xml_attr_string(connectorNode, "name", &vectorPortInput->name, SSD_MANDATORY);
        if (RETURN_ERROR == retVal) {
            goto cleanup;
        }

        retVal = xml_attr_string(connectorNode, "description", &vectorPortInput->description, SSD_OPTIONAL);
        if (RETURN_ERROR == retVal) {
            goto cleanup;
        }

        {
            xmlNodePtr realNode = xml_child(connectorNode, "Real");
            if (realNode) {
                char * unitName = NULL;
                SSDUnit * unit = NULL;

                retVal = xml_attr_string(realNode, "unit", &unitName, SSD_OPTIONAL);
                if (RETURN_ERROR == retVal) {
                    goto cleanup;
                }

                if (unitName) {
                    unit = (SSDUnit*)units->GetByName(units, unitName);
                    if (!unit) {
                        retVal = xml_error_generic(realNode, "Referenced unit %s is not defined", unitName);
                        goto cleanup_1;
                    }

                    vectorPortInput->unit = mcx_string_copy(unit->name);
                    if (!vectorPortInput->unit) {
                        retVal = RETURN_ERROR;
                        goto cleanup_1;
                    }
cleanup_1:
                    if (unitName) { mcx_free(unitName); }

                    if (retVal == RETURN_ERROR) {
                        goto cleanup;
                    }
                } else {
                    vectorPortInput->unit = mcx_string_copy(DEFAULT_NO_UNIT);
                    if (!vectorPortInput->unit) {
                        retVal = RETURN_ERROR;
                        goto cleanup;
                    }
                }
            }
        }

        {
            xmlNodePtr typeNode = xml_child_by_index(connectorNode, 0);

            char * typeName = NULL;
            size_t i = 0;

            if (!typeNode) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            typeName = xml_node_get_name(typeNode);
            if (!typeName) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            for (i = 0; _typeMapping[i].key; i++) {
                if (!strcmp(typeName, _typeMapping[i].key)) {
                    vectorPortInput->type = _typeMapping[i].value;
                    break;
                }
            }
            if (!ChannelTypeIsValid(vectorPortInput->type)) {
                retVal = xml_error_unsupported_node(typeNode);
                goto cleanup;
            }
        }

        {
            xmlNodePtr annotationNode = xml_annotation_of_type(connectorNode, "com.avl.model.connect.ssp.port");

            if (annotationNode) {

                xmlNodePtr portNode = xml_child(annotationNode, "Port");

                if (!portNode) {
                    retVal = xml_error_missing_child(annotationNode, "Port");
                    goto cleanup;
                }

                retVal = xml_attr_string(portNode, "nameInModel", &vectorPortInput->nameInModel, SSD_OPTIONAL);
                if (RETURN_ERROR == retVal) {
                    goto cleanup;
                }

                retVal = xml_attr_string(portNode, "id", &vectorPortInput->id, SSD_OPTIONAL);
                if (RETURN_ERROR == retVal) {
                    goto cleanup;
                }

                {
                    xmlNodePtr vectorNode = NULL;
                    size_t num = 0;

                    switch (vectorPortInput->type->con) {
                    case CHANNEL_DOUBLE:
                        vectorNode = xml_child(portNode, "RealVector");
                        break;
                    case CHANNEL_INTEGER:
                        vectorNode = xml_child(portNode, "IntegerVector");
                        break;
                    case CHANNEL_BOOL:
                        vectorNode = xml_child(portNode, "BooleanVector");
                        break;
                    default:
                        mcx_log(LOG_ERROR, "SSDReadComponentVectorPort: Unsupported type: %s", ChannelTypeToString(vectorPortInput->type));
                        retVal = RETURN_ERROR;
                        goto cleanup;
                    }

                    if (!vectorNode) {
                        // startIndex is MANDATORY
                        retVal = RETURN_ERROR;
                        goto cleanup;
                    }

                    {
                        retVal = xml_attr_int64(vectorNode, "startIndex", &vectorPortInput->startIndex, SSD_MANDATORY);
                        if (RETURN_ERROR == retVal) {
                            goto cleanup;
                        }

                        retVal = xml_attr_int64(vectorNode, "endIndex", &vectorPortInput->endIndex, SSD_MANDATORY);
                        if (RETURN_ERROR == retVal) {
                            goto cleanup;
                        }

                        num = (size_t) (vectorPortInput->endIndex - vectorPortInput->startIndex + 1);
                    }

                    retVal = xml_attr_vec_len(vectorNode, "min", vectorPortInput->type, num, &vectorPortInput->min, SSD_OPTIONAL);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_attr_vec_len(vectorNode, "max", vectorPortInput->type, num, &vectorPortInput->max, SSD_OPTIONAL);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_attr_vec_len(vectorNode, "scale", vectorPortInput->type, num, &vectorPortInput->scale, SSD_OPTIONAL);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_attr_vec_len(vectorNode, "offset", vectorPortInput->type, num, &vectorPortInput->offset, SSD_OPTIONAL);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_attr_vec_len(vectorNode, "default", vectorPortInput->type, num, &vectorPortInput->default_, SSD_OPTIONAL);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_attr_vec_len(vectorNode, "initial", vectorPortInput->type, num, &vectorPortInput->initial, SSD_OPTIONAL);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_bool32(vectorNode, "writeResults", &vectorPortInput->writeResults);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }
                }
            }
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(vectorPortInput);
        return NULL;
    }

    return vectorPortInput;
}

ScalarPortInput * SSDReadComponentScalarPort(xmlNodePtr connectorNode, const char * type, ObjectContainer * units) {
    ScalarPortInput * scalarPortInput = (ScalarPortInput *) object_create(ScalarPortInput);
    InputElement * element = (InputElement *) scalarPortInput;

    McxStatus retVal = RETURN_OK;

    if (!scalarPortInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) connectorNode;

    {
        retVal = xml_attr_string(connectorNode, "name", &scalarPortInput->name, SSD_MANDATORY);
        if (RETURN_ERROR == retVal) {
            goto cleanup;
        }

        retVal = xml_attr_string(connectorNode, "description", &scalarPortInput->description, SSD_OPTIONAL);
        if (RETURN_ERROR == retVal) {
            goto cleanup;
        }

        {
            xmlNodePtr realNode = xml_child(connectorNode, "Real");
            if (realNode) {
                char * unitName = NULL;
                SSDUnit * unit = NULL;

                retVal = xml_attr_string(realNode, "unit", &unitName, SSD_OPTIONAL);
                if (RETURN_ERROR == retVal) {
                    goto cleanup;
                }

                if (unitName) {
                    unit = (SSDUnit*)units->GetByName(units, unitName);
                    if (!unit) {
                        retVal = xml_error_generic(realNode, "Referenced unit %s is not defined", unitName);
                        goto cleanup_1;
                    }

                    scalarPortInput->unit = mcx_string_copy(unit->name);
                    if (!scalarPortInput->unit) {
                        retVal = RETURN_ERROR;
                        goto cleanup_1;
                    }
cleanup_1:
                    if (unitName) { mcx_free(unitName); }

                    if (retVal == RETURN_ERROR) {
                        goto cleanup;
                    }
                } else {
                    scalarPortInput->unit = mcx_string_copy(DEFAULT_NO_UNIT);
                    if (!scalarPortInput->unit) {
                        retVal = RETURN_ERROR;
                        goto cleanup;
                    }
                }
            }
        }

        {
            xmlNodePtr typeNode = xml_child_by_index(connectorNode, 0);

            char * typeName = NULL;
            size_t i = 0;

            if (!typeNode) {
                // SSP says if no type is supplied, the default mechanisms are to be used. For FMUs
                // this means reading the type from the modelDescriptions.xml. Since we cannot do
                // this here, we report this as an error.
                retVal = xml_error_generic(connectorNode, "Element is missing child node specifying the type, e.g. Real");
                goto cleanup;
            }

            typeName = xml_node_get_name(typeNode);
            if (!typeName) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            for (i = 0; _typeMapping[i].key; i++) {
                if (!strcmp(typeName, _typeMapping[i].key)) {
                    scalarPortInput->type = _typeMapping[i].value;
                    break;
                }
            }
            if (!ChannelTypeIsValid(scalarPortInput->type)) {
                retVal = xml_error_unsupported_node(typeNode);
                goto cleanup;
            }
        }

        {
            xmlNodePtr annotationNode = xml_annotation_of_type(connectorNode, "com.avl.model.connect.ssp.port");

            if (annotationNode) {

                xmlNodePtr portNode = xml_child(annotationNode, "Port");

                if (!portNode) {
                    retVal = xml_error_missing_child(annotationNode, "Port");
                    goto cleanup;
                }

                retVal = xml_attr_string(portNode, "nameInModel", &scalarPortInput->nameInModel, SSD_OPTIONAL);
                if (RETURN_ERROR == retVal) {
                    goto cleanup;
                }

                retVal = xml_attr_string(portNode, "id", &scalarPortInput->id, SSD_OPTIONAL);
                if (RETURN_ERROR == retVal) {
                    goto cleanup;
                }

                {
                    xmlNodePtr scalarNode = NULL;
                    size_t num = 0;

                    switch (scalarPortInput->type->con) {
                    case CHANNEL_DOUBLE:
                        scalarNode = xml_child(portNode, "Real");
                        break;
                    case CHANNEL_INTEGER:
                        scalarNode = xml_child(portNode, "Integer");
                        break;
                    case CHANNEL_BOOL:
                        scalarNode = xml_child(portNode, "Boolean");
                        break;
                    case CHANNEL_BINARY:
                        scalarNode = xml_child(portNode, "Binary");
                        if (!scalarNode) {
                            scalarNode = xml_child(portNode, "BinaryReference");
                        }
                        break;
                    default:
                        mcx_log(LOG_ERROR, "SSDReadComponentScalarPort: Unsupported type: %s", ChannelTypeToString(scalarPortInput->type));
                        retVal = RETURN_ERROR;
                        goto cleanup;
                    }

                    if (!scalarNode) {
                        // all attributes are OPTIONAL -> no error
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_channel_value_data(scalarNode, "min", scalarPortInput->type, &scalarPortInput->min);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_channel_value_data(scalarNode, "max", scalarPortInput->type, &scalarPortInput->max);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_channel_value_data(scalarNode, "scale", scalarPortInput->type, &scalarPortInput->scale);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_channel_value_data(scalarNode, "offset", scalarPortInput->type, &scalarPortInput->offset);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_channel_value_data(scalarNode, "default", scalarPortInput->type, &scalarPortInput->default_);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_channel_value_data(scalarNode, "initial", scalarPortInput->type, &scalarPortInput->initial);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }

                    retVal = xml_opt_attr_bool32(scalarNode, "writeResults", &scalarPortInput->writeResults);
                    if (RETURN_ERROR == retVal) {
                        goto cleanup;
                    }
                }
            }
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(scalarPortInput);
        return NULL;
    }

    return scalarPortInput;
}

McxStatus SSDReadComponentPort(PortInput * portInput, xmlNodePtr connectorNode, const char * type, ObjectContainer * units) {
    McxStatus retVal = RETURN_OK;

    {
        xmlNodePtr annotationNode = xml_annotation_of_type(connectorNode, "com.avl.model.connect.ssp.port");

        if (annotationNode) {
            xmlNodePtr portNode = xml_child(annotationNode, "Port");

            if (!portNode) {
                return xml_error_missing_child(annotationNode, "Port");
            }

            retVal = xml_validate_node(portNode, "com.avl.model.connect.ssp.port");
            if (retVal == RETURN_ERROR) {
                return RETURN_ERROR;
            }

            if (SSDConnectorIsVectorPort(connectorNode)) {
                portInput->type = PORT_VECTOR;
                portInput->port.vectorPort = SSDReadComponentVectorPort(connectorNode, type, units);

                if (!portInput->port.vectorPort) {
                    return RETURN_ERROR;
                }
            } else {
                portInput->type = PORT_SCALAR;
                portInput->port.scalarPort = SSDReadComponentScalarPort(connectorNode, type, units);

                if (!portInput->port.scalarPort) {
                    return RETURN_ERROR;
                }
            }
        } else {
            portInput->type = PORT_SCALAR;
            portInput->port.scalarPort = SSDReadComponentScalarPort(connectorNode, type, units);

            if (!portInput->port.scalarPort) {
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

PortsInput * SSDReadComponentPorts(xmlNodePtr connectorsNode,
                                   const char * type,
                                   const ObjectClass * portClass,
                                   const char * annotationType,
                                   fReadSSDPortSpecificData readPort,
                                   ObjectContainer * units) {
    PortsInput * portsInput = (PortsInput *) object_create(PortsInput);
    InputElement * element = (InputElement *) portsInput;

    McxStatus retVal = RETURN_OK;

    if (!portsInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) connectorsNode;

    {
        size_t i = 0;
        size_t num = xml_num_children(connectorsNode);

        for (i = 0; i < num; i++) {
            xmlNodePtr connectorNode = xml_child_by_index(connectorsNode, i);
            char * connectorType = NULL;

            PortInput * portInput = NULL;
            InputElement * portElement = NULL;

            retVal = xml_attr_string(connectorNode, "kind", &connectorType, SSD_MANDATORY);
            if (RETURN_OK != retVal) {
                goto cleanup_1;
            }

            if (strcmp(type, connectorType)) {
                goto cleanup_1;
            }

            // construct the port input object
            portInput = (PortInput *) object_create_(portClass);
            if (!portInput) {
                goto cleanup_1;
            }

            portElement = (InputElement*)portInput;
            portElement->type = INPUT_SSD;
            portElement->context = (void*)connectorNode;

            // read common port data
            retVal = SSDReadComponentPort(portInput, connectorNode, type, units);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            // read specific port data
            {
                if (annotationType && readPort) {
                    xmlNodePtr annotationNode = xml_annotation_of_type(connectorNode, annotationType);

                    if (annotationNode) {
                        xmlNodePtr specificDataNode = xml_child(annotationNode, "SpecificData");
                        if (!specificDataNode) {
                            retVal = xml_error_missing_child(annotationNode, "SpecificData");
                            goto cleanup_1;
                        }

                        retVal = xml_validate_node(specificDataNode, annotationType);
                        if (retVal == RETURN_ERROR) {
                            goto cleanup_1;
                        }

                        retVal = readPort(specificDataNode, portInput);
                        if (retVal == RETURN_ERROR) {
                            goto cleanup_1;
                        }
                    } else {
                        retVal = xml_error_generic(connectorNode, "Annotation %s not defined", annotationType);
                        goto cleanup_1;
                    }
                }
            }

            retVal = portsInput->ports->PushBack(portsInput->ports, (Object *) portInput);
            if (RETURN_OK != retVal) {
                goto cleanup_1;
            }

       cleanup_1:
            if (connectorType) { mcx_free(connectorType); }

            if (RETURN_ERROR == retVal) {
                object_destroy(portInput);
                goto cleanup_0;
            }
        }

    }

cleanup_0:
    if (retVal == RETURN_ERROR) {
        object_destroy(portsInput);
        return NULL;
    }

    return portsInput;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */