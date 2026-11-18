/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Parameters.h"
#include "reader/ssp/Schema.h"
#include "reader/ssp/Units.h"

#include "reader/model/parameters/specific_data/FmuParameterInput.h"
#include "reader/model/components/specific_data/FmuInput.h"

#include "reader/EnumMapping.h"

#include "util/string.h"
#include "objects/Object.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct SSDParameterValue SSDParameterValue;

typedef McxStatus(*fSSDParameterValueSetup)(SSDParameterValue * paramValue, xmlNodePtr node, ObjectContainer * units);

struct SSDParameterValue {
    Object _;

    fSSDParameterValueSetup Setup;

    char * name;
    char * baseName;

    size_t numDims;
    size_t idx1;
    size_t idx2;

    SSDUnit * unit;

    xmlNodePtr node;
};


static McxStatus ExtractLastIndex(char * name, size_t * index) {
    // for easier understanding of the code, suppose that name is of the form param[1][0][3]
    // we want to extract 3 and truncate name to param[1][0]
    size_t len = strlen(name);
    char * bracketStart = NULL;
    int ret = 0;

    size_t idx = 0;

    if (len <= 3) {
        return RETURN_ERROR;
    }

    // check that there are brackets at the end of of name
    if (name[len - 1] != ']') {
        return RETURN_ERROR;
    }

    // find the first '[' from the right
    bracketStart = strrchr(name, '[');
    if (!bracketStart) {
        return RETURN_ERROR;
    }

    // brackets were found, check that between them there is actually a number
    ret = sscanf(bracketStart, "[%zu]", &idx);
    if (ret != 1) {
        return RETURN_ERROR;
    }

    // success: forward the index and truncate the name
    *index = idx;
    *bracketStart = '\0';

    return RETURN_OK;
}


static McxStatus SSDParameterValueSetup(SSDParameterValue * paramValue, xmlNodePtr node, ObjectContainer * units) {
    McxStatus retVal = RETURN_OK;
    size_t swap = 0;

    retVal = xml_attr_string(node, "name", &paramValue->name, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    if (!strcmp(paramValue->name, "")) {
        xml_error_generic(node, "Parameter name can not be empty");
        return RETURN_ERROR;
    }

    {
        // read unit
        xmlNodePtr realNode = xml_child(node, "Real");
        if (realNode) {
            char * unitName = NULL;

            retVal = xml_attr_string(realNode, "unit", &unitName, SSD_OPTIONAL);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            if (unitName) {
                paramValue->unit = (SSDUnit*)units->GetByName(units, unitName);
                if (!paramValue->unit) {
                    retVal = xml_error_generic(realNode, "Referenced unit %s is not defined", unitName);
                    goto cleanup;
                }

                paramValue->unit = paramValue->unit->Clone(paramValue->unit);
                if (!paramValue->unit) {
                    retVal = RETURN_ERROR;
                    goto cleanup;
                }

cleanup:
                if (unitName) { mcx_free(unitName); }

                if (retVal == RETURN_ERROR) {
                    return RETURN_ERROR;
                }
            }
        }
    }

    paramValue->node = node;

    paramValue->baseName = mcx_string_copy(paramValue->name);
    if (!paramValue->baseName) {
        return RETURN_ERROR;
    }

    paramValue->numDims = paramValue->idx1 = paramValue->idx2 = 0;

    // extract last index
    retVal = ExtractLastIndex(paramValue->baseName, &paramValue->idx1);
    if (retVal == RETURN_ERROR) {
        return RETURN_OK;
    }

    paramValue->numDims++;

    // extract second to last index
    retVal = ExtractLastIndex(paramValue->baseName, &paramValue->idx2);
    if (retVal == RETURN_ERROR) {
        return RETURN_OK;
    }

    paramValue->numDims++;

    // swap the indices (because we stored them in reverse order)
    swap = paramValue->idx1;
    paramValue->idx1 = paramValue->idx2;
    paramValue->idx2 = swap;

    return RETURN_OK;
}


static void SSDParameterValueDestructor(SSDParameterValue * paramValue) {
    if (paramValue->name) { mcx_free(paramValue->name); }
    if (paramValue->baseName) { mcx_free(paramValue->baseName); }
    if (paramValue->unit) { object_destroy(paramValue->unit); }
}

static SSDParameterValue * SSDParameterValueCreate(SSDParameterValue * paramValue) {
    paramValue->name = NULL;
    paramValue->baseName = NULL;
    paramValue->node = NULL;

    paramValue->numDims = 0;
    paramValue->idx1 = 0;
    paramValue->idx2 = 0;

    paramValue->unit = NULL;

    paramValue->Setup = SSDParameterValueSetup;

    return paramValue;
}

OBJECT_CLASS(SSDParameterValue, Object);

typedef struct SSDParameter SSDParameter;

typedef McxStatus(*fSSDParameterAddValue)(SSDParameter * param, SSDParameterValue * paramValue);

struct SSDParameter {
    Object _;

    fSSDParameterAddValue AddValue;

    char * name;
    int isArray;
    xmlNodePtr connectorNode;

    SSDUnit * connectorUnit;    // weak reference

    ObjectContainer * paramValues;
};

static McxStatus SSDParameterAddValue(SSDParameter * param, SSDParameterValue * paramValue) {
    McxStatus retVal = RETURN_OK;

    int idx = param->paramValues->GetNameIndex(param->paramValues, paramValue->name);

    if (idx != -1) {
        SSDParameterValue * existingValue  = (SSDParameterValue*)param->paramValues->At(param->paramValues, idx);

        xml_warning_generic(existingValue->node, "Parameter value is overriden by the value on line %d", xml_node_line_number(paramValue->node));

        object_destroy(existingValue);
        retVal = param->paramValues->SetAt(param->paramValues, idx, (Object*)paramValue);
    } else {
        retVal = param->paramValues->PushBackNamed(param->paramValues, (Object*)paramValue, paramValue->name);
    }

    return retVal;
}

static void SSDParameterDestructor(SSDParameter * param) {
    if (param->name) { mcx_free(param->name); }

    if (param->paramValues) {
        param->paramValues->DestroyObjects(param->paramValues);
        object_destroy(param->paramValues);
    }
}

static SSDParameter * SSDParameterCreate(SSDParameter * param) {
    param->name = NULL;
    param->connectorNode = NULL;
    param->connectorUnit = NULL;
    param->isArray = FALSE;

    param->paramValues = object_create(ObjectContainer);
    if (!param->paramValues) {
        return NULL;
    }

    param->AddValue = SSDParameterAddValue;

    return param;
}

OBJECT_CLASS(SSDParameter, Object);


static McxStatus IsParameterBindingSupported(xmlNodePtr paramBindingNode) {
    McxStatus retVal = RETURN_OK;

    char * type = NULL;
    char * source = NULL;
    char * prefix = NULL;

    // only SSV parameter value definitions are supported
    retVal = xml_attr_string(paramBindingNode, "type", &type, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    if (type && strcmp(type, "application/x-ssp-parameter-set")) {
        retVal = xml_warning_generic(paramBindingNode, "Parameter binding type %s is not supported. "
                                     "Parameter binding is ignored", type);
        goto cleanup;
    }

    // only inline parameter value definitions are supported
    retVal = xml_attr_string(paramBindingNode, "source", &source, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    if (source) {
        retVal = xml_warning_generic(paramBindingNode, "External parameter sources are not supported. "
                                     "Parameter binding is ignored");
        goto cleanup;
    }

    // prefix definition is not supported (as it is only useful in combination with hierarchies)
    retVal = xml_attr_string(paramBindingNode, "prefix", &prefix, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    if (prefix && strcmp("", prefix)) {
        retVal = xml_warning_generic(paramBindingNode, "Parameter name resolution prefix is not supported. "
                                     "Parameter binding is ignored");
        goto cleanup;
    }

    {
        // parameter mappings are ignored
        xmlNodePtr mappingNode = xml_child(paramBindingNode, "ParameterMapping");
        if (mappingNode) {
            xml_warning_generic(mappingNode, "Parameter mappings are not supported. Parameter mapping is ignored");
        }
    }

    retVal = RETURN_OK;

cleanup:
    if (type) { mcx_free(type); }
    if (source) { mcx_free(source); }

    return retVal;
}


static ObjectContainer * CollectParameterConnectors(xmlNodePtr connectorsNode, ObjectContainer * units) {
    McxStatus retVal = RETURN_OK;

    size_t i = 0;
    size_t numConnectors = xml_num_children(connectorsNode);

    ObjectContainer * parameters = (ObjectContainer*)object_create(ObjectContainer);
    if (!parameters) {
        return NULL;
    }

    for (i = 0; i < numConnectors; i++) {
        xmlNodePtr connectorNode = xml_child_by_index(connectorsNode, i);
        SSDParameter * parameter = NULL;
        char * connectorType = NULL;
        int idx = -1;

        retVal = xml_attr_string(connectorNode, "kind", &connectorType, SSD_MANDATORY);
        if (RETURN_OK != retVal) {
            goto cleanup_1;
        }

        if (strcmp(connectorType, "parameter")) {
            goto cleanup_1;
        }

        parameter = (SSDParameter *)object_create(SSDParameter);
        if (!parameter) {
            retVal = RETURN_ERROR;
            goto cleanup_1;
        }

        retVal = xml_attr_string(connectorNode, "name", &parameter->name, SSD_MANDATORY);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        {
            // check if it is denoted as an array parameter
            parameter->isArray = FALSE;

            xmlNodePtr annotationNode = xml_annotation_of_type(connectorNode, "com.avl.model.connect.ssp.parameter");
            if (annotationNode) {
                xmlNodePtr parameterNode = xml_child(annotationNode, "Parameter");

                if (!parameterNode) {
                    retVal = xml_error_missing_child(annotationNode, "Parameter");
                    goto cleanup_1;
                }

                retVal = xml_validate_node(parameterNode, "com.avl.model.connect.ssp.parameter");
                if (retVal == RETURN_ERROR) {
                    goto cleanup_1;
                }

                parameter->isArray = TRUE;
            }
        }

        {
            // get the unit definition reference
            xmlNodePtr realNode = xml_child(connectorNode, "Real");
            if (realNode) {
                char * unitName = NULL;

                retVal = xml_attr_string(realNode, "unit", &unitName, SSD_OPTIONAL);
                if (RETURN_ERROR == retVal) {
                    goto cleanup_1;
                }

                if (unitName) {
                    parameter->connectorUnit = (SSDUnit*)units->GetByName(units, unitName);
                    if (!parameter->connectorUnit) {
                        retVal = xml_error_generic(realNode, "Referenced unit %s is not defined", unitName);
                        goto cleanup_2;
                    }

cleanup_2:
                    if (unitName) { mcx_free(unitName); }

                    if (retVal == RETURN_ERROR) {
                        goto cleanup_1;
                    }
                }
            }
        }

        parameter->connectorNode = connectorNode;

        idx = parameters->GetNameIndex(parameters, parameter->name);
        if (idx != -1) {
            retVal = xml_error_generic(connectorNode, "Connector %s already defined", parameter->name);
            goto cleanup_1;
        }

        retVal = parameters->PushBackNamed(parameters, (Object *) parameter, parameter->name);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

cleanup_1:
        if (connectorType) { mcx_free(connectorType); }

        if (retVal == RETURN_ERROR) {
            object_destroy(parameter);
            goto cleanup_0;
        }
    }

cleanup_0:
    if (retVal == RETURN_ERROR) {
        if (parameters) {
            parameters->DestroyObjects(parameters);
            object_destroy(parameters);
        }
    }

    return parameters;
}


static int SSDCompatibleTypes(SSDParameter * parameter, SSDParameterValue * paramValue) {
    xmlNodePtr valueType = xml_child_by_index(paramValue->node, 0);
    xmlNodePtr connectorType = xml_child_by_index(parameter->connectorNode, 0);

    int retVal = !strcmp(xml_node_get_name(valueType), xml_node_get_name(connectorType));

    if (!retVal) {
        xml_warning_generic(paramValue->node, "No matching connector found for parameter %s "
                            "(connector %s and parameter %s have incompatible types)", paramValue->name, parameter->name, paramValue->name);
    }

    return retVal;
}


static int SSDCompatibleUnits(SSDParameter * parameter, SSDParameterValue * paramValue) {
    if (!paramValue->unit || !parameter->connectorUnit) {
        return TRUE;
    }

    if (paramValue->unit && !strcmp(paramValue->unit->name, DEFAULT_NO_UNIT)) {
        return TRUE;
    }

    if (parameter->connectorUnit && !strcmp(parameter->connectorUnit->name, DEFAULT_NO_UNIT)) {
        return TRUE;
    }

    if (strcmp(parameter->connectorUnit->name, paramValue->unit->name)) {
        xml_warning_generic(paramValue->node, "No matching connector found for parameter %s "
                            "(connector %s and parameter %s have incompatible units %s - %s)",
                            paramValue->name, parameter->name, paramValue->name,
                            parameter->connectorUnit->name, paramValue->unit->name);
        return FALSE;
    }
    return TRUE;
}


static McxStatus CollectParameterValues(xmlNodePtr paramBindingsNode, ObjectContainer * parameters) {
    McxStatus retVal = RETURN_OK;

    size_t i = 0;
    size_t numBindings = xml_num_children(paramBindingsNode);

    for (i = 0; i < numBindings; i++) {
        xmlNodePtr paramBindingNode = xml_child_by_index(paramBindingsNode, i);
        xmlNodePtr paramValuesNode = xml_child(paramBindingNode, "ParameterValues");
        xmlNodePtr paramSetNode = NULL;
        xmlNodePtr parametersNode = NULL;
        ObjectContainer * units = NULL;

        size_t j = 0;
        size_t numParamValues = 0;

        retVal = IsParameterBindingSupported(paramBindingNode);
        if (retVal == RETURN_ERROR) {
            goto cleanup_0;
        } else if (retVal == RETURN_WARNING) {
            continue;
        }

        if (!paramValuesNode) {
            continue;
        }

        paramSetNode = xml_child(paramValuesNode, "ParameterSet");
        if (!paramSetNode) {
            retVal = xml_error_missing_child(paramValuesNode, "ParameterSet");
            goto cleanup_0;
        }

        retVal = xml_validate_node(paramSetNode, "SystemStructureParameterValues.xsd");
        if (retVal == RETURN_ERROR) {
            goto cleanup_0;
        }

        units = SSDReadUnits(xml_child(paramSetNode, "Units"));
        if (!units) {
            retVal = RETURN_ERROR;
            goto cleanup_1;
        }

        parametersNode = xml_child(paramSetNode, "Parameters");
        numParamValues = xml_num_children(parametersNode);

        for (j = 0; j < numParamValues; j++) {
            SSDParameter * parameter = NULL;
            xmlNodePtr paramNode = xml_child_by_index(parametersNode, j);
            SSDParameterValue * paramValue = (SSDParameterValue*)object_create(SSDParameterValue);

            if (!paramValue) {
                retVal = RETURN_ERROR;
                goto cleanup_2;
            }

            retVal = paramValue->Setup(paramValue, paramNode, units);
            if (retVal == RETURN_ERROR) {
                goto cleanup_2;
            }

            // matching by the full name has precedence
            parameter = (SSDParameter*)parameters->GetByName(parameters, paramValue->name);
            if (!parameter || parameter->isArray || !SSDCompatibleTypes(parameter, paramValue) || !SSDCompatibleUnits(parameter, paramValue)) {
                // full name match failed -> match by the base name
                parameter = (SSDParameter*)parameters->GetByName(parameters, paramValue->baseName);

                if (!parameter) {
                    // create a new parameter here
                    parameter = (SSDParameter *)object_create(SSDParameter);
                    if (!parameter) {
                        retVal = RETURN_ERROR;
                        goto cleanup_2;
                    }

                    parameter->name = mcx_string_copy(paramValue->name);
                    parameter->isArray = FALSE;

                    if (paramValue->unit) {
                        parameter->connectorUnit = (SSDUnit*)units->GetByName(units, paramValue->unit->name);
                        if (!parameter->connectorUnit) {
                            retVal = xml_error_generic(paramNode, "Referenced unit %s is not defined", paramValue->unit);
                            goto cleanup_2;
                        }
                    }

                    parameter->connectorNode = paramNode; // duck typing: used attributes for both nodes are the same

                    retVal = parameters->PushBackNamed(parameters, (Object *)parameter, parameter->name);
                    if (retVal == RETURN_ERROR) {
                        goto cleanup_2;
                    }
                }

                if (!SSDCompatibleTypes(parameter, paramValue)) {
                    retVal = RETURN_WARNING;
                    goto cleanup_2;
                }

                if (!SSDCompatibleUnits(parameter, paramValue)) {
                    retVal = RETURN_WARNING;
                    goto cleanup_2;
                }
            }

            retVal = parameter->AddValue(parameter, paramValue);
            if (retVal == RETURN_ERROR) {
                goto cleanup_2;
            }

cleanup_2:
            if (retVal != RETURN_OK) {
                object_destroy(paramValue);
            }

            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }
        }

cleanup_1:
        object_destroy(units);

        if (retVal == RETURN_ERROR) {
            goto cleanup_0;
        }
    }

cleanup_0:
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static MapStringChannelType _typeMappingScalar[] = {
    {"Real",        &ChannelTypeDouble},
    {"Integer",     &ChannelTypeInteger},
    {"Boolean",     &ChannelTypeBool},
    {"String",      &ChannelTypeString},
    {NULL,          &ChannelTypeUnknown},
};

static ScalarParameterInput * SSDReadScalarParameter(SSDParameter * parameter) {
    ScalarParameterInput * input = (ScalarParameterInput *) object_create(ScalarParameterInput);
    InputElement * element = (InputElement *) input;

    size_t numParamValues = parameter->paramValues->Size(parameter->paramValues);
    SSDParameterValue * paramValue = NULL;

    xmlNodePtr connectorTypeNode = NULL;
    xmlNodePtr parameterTypeNode = NULL;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) parameter->connectorNode;

    if (numParamValues == 0) {
        retVal = xml_error_generic(parameter->connectorNode, "No value defined for parameter %s", parameter->name);
        goto cleanup;
    }

    paramValue = (SSDParameterValue*)parameter->paramValues->At(parameter->paramValues, numParamValues - 1);

    input->name = mcx_string_copy(parameter->name);
    if (!input->name) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    connectorTypeNode = xml_child_by_index(parameter->connectorNode, 0);
    parameterTypeNode = xml_child_by_index(paramValue->node, 0);

    {
        char * connectorType = xml_node_get_name(connectorTypeNode);
        char * parameterType = xml_node_get_name(parameterTypeNode);

        size_t i = 0;

        if (strcmp(parameterType, connectorType)) {
            retVal = xml_error_generic(parameter->connectorNode, "Type mismatch %s - %s (line: %d)",
                                       connectorType, parameterType, xml_node_line_number(parameterTypeNode));
            goto cleanup;
        }

        for (i = 0; _typeMappingScalar[i].key; i++) {
            if (!strcmp(connectorType, _typeMappingScalar[i].key)) {
                input->type = _typeMappingScalar[i].value;
                break;
            }
        }

        if (!ChannelTypeIsValid(input->type)) {
            retVal = xml_error_unsupported_node(connectorTypeNode);
            goto cleanup;
        }
    }

    retVal = xml_attr_channel_value_data(parameterTypeNode, "value", input->type, &input->value.value, SSD_MANDATORY);
    if (RETURN_ERROR == retVal) {
        goto cleanup;
    }
    input->value.defined = TRUE;

    // read unit
    if (parameter->connectorUnit) {
        input->unit = mcx_string_copy(parameter->connectorUnit->name);
        if (!input->unit && parameter->connectorUnit->name) {
            retVal = RETURN_ERROR;
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

static MapStringChannelType _typeMappingArray[] = {
    {"Real",        &ChannelTypeDouble},
    {"Integer",     &ChannelTypeInteger},
    {"Boolean",     &ChannelTypeBool},
    {NULL,          &ChannelTypeUnknown},
};

static ArrayParameterDimensionInput * SSDReadArrayParameterDimension(xmlNodePtr node) {
    ArrayParameterDimensionInput * input = (ArrayParameterDimensionInput *) object_create(ArrayParameterDimensionInput);
    InputElement * element = (InputElement *) input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) node;


    retVal = xml_attr_int(node, "start", &input->start, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_attr_int(node, "end", &input->end, SSD_MANDATORY);
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

static McxStatus AllocateMemory(SSDParameter * parameter, ChannelType * type, size_t numValues, void ** dest) {
    size_t size = 0;

    switch (type->con) {
        case CHANNEL_DOUBLE:
            size = sizeof(double);
            break;
        case CHANNEL_INTEGER:
        case CHANNEL_BOOL:
            size = sizeof(int);
            break;
        default:
            return xml_error_generic(parameter->connectorNode, "Unsupported connector type");
    }

    *dest = mcx_calloc(numValues, size);
    if (!(*dest)) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus SSDReadDimensionlessArrayParameter(SSDParameter * parameter, ArrayParameterInput * input) {
    McxStatus retVal = RETURN_OK;

    size_t i = 0;
    SSDParameterValue * paramValue = NULL;

    size_t numParamValues = parameter->paramValues->Size(parameter->paramValues);

    // buffer for constructing names
    char * name = (char*)mcx_calloc(strlen(parameter->name) + 13, sizeof(char));    // 13 is enough to store '[<some_int>]\0'
    if (!name) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    // determine how long the array is (starting index must be 0)
    while (1) {
        int ret = sprintf(name, "%s[%zu]", parameter->name, i);
        if (ret < 0) {
            retVal = xml_error_generic(parameter->connectorNode, "Error finding parameter values");
            goto cleanup;
        }

        paramValue = (SSDParameterValue*)parameter->paramValues->GetByName(parameter->paramValues, name);
        if (!paramValue) {
            break;
        }

        i++;
    }

    input->numValues = i;

    // allocate the memory to store the values
    retVal = AllocateMemory(parameter, input->type, input->numValues, &input->values);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    // read parameter values
    for (i = 0; i < numParamValues; i++) {
        xmlNodePtr paramTypeNode = NULL;

        paramValue = (SSDParameterValue *)parameter->paramValues->At(parameter->paramValues, i);
        paramTypeNode = xml_child_by_index(paramValue->node, 0);

        if (paramValue->numDims != 1) {
            xml_warning_generic(paramValue->node, "No matching connector found for parameter %s", paramValue->name);
            continue;
        }

        if (paramValue->idx1 > input->numValues) {
            xml_warning_generic(paramValue->node, "No matching connector found for parameter %s", paramValue->name);
            continue;
        }

        switch (input->type->con) {
            case CHANNEL_DOUBLE:
                retVal = xml_attr_double(paramTypeNode, "value", (double*)input->values + paramValue->idx1, SSD_MANDATORY);
                break;
            case CHANNEL_INTEGER:
                retVal = xml_attr_int(paramTypeNode, "value", (int*)input->values + paramValue->idx1, SSD_MANDATORY);
                break;
            case CHANNEL_BOOL:
                retVal = xml_attr_bool(paramTypeNode, "value", (int*)input->values + paramValue->idx1, SSD_MANDATORY);
                break;
            default:
                break;
        }

        if (RETURN_ERROR == retVal) {
            goto cleanup;
        }
    }

cleanup:
    if (name) { mcx_free(name); }

    return retVal;
}

static McxStatus SSDReadDimensionArrayParameter(SSDParameter * parameter, ArrayParameterInput * input) {
    McxStatus retVal = RETURN_OK;

    size_t i = 0;
    SSDParameterValue * paramValue = NULL;
    int * filledMask = NULL;

    size_t numParamValues = parameter->paramValues->Size(parameter->paramValues);

    // allocate memory to store the values
    input->numValues = 1;

    for (i = 0; i < input->numDims; i++) {
        input->numValues *= (input->dims[i]->end - input->dims[i]->start + 1);
    }

    retVal = AllocateMemory(parameter, input->type, input->numValues, &input->values);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    // structure for checking that all values are present
    filledMask = (int*)mcx_calloc(input->numValues, sizeof(int));
    if (!filledMask) {
        return RETURN_ERROR;
    }

    // read parameter values
    for (i = 0; i < numParamValues; i++) {
        xmlNodePtr paramTypeNode = NULL;
        size_t index = 0;

        paramValue = (SSDParameterValue *)parameter->paramValues->At(parameter->paramValues, i);
        paramTypeNode = xml_child_by_index(paramValue->node, 0);

        if (paramValue->numDims != input->numDims) {
            xml_warning_generic(paramValue->node, "No matching connector found for parameter %s", paramValue->name);
            continue;
        }

        if (paramValue->numDims == 1) {
            if (paramValue->idx1 > input->dims[0]->end || paramValue->idx1 < input->dims[0]->start) {
                xml_warning_generic(paramValue->node, "No matching connector found for parameter %s", paramValue->name);
                continue;
            }

            index = paramValue->idx1 - input->dims[0]->start;
        } else {
            if (paramValue->idx1 > input->dims[0]->end || paramValue->idx1 < input->dims[0]->start) {
                xml_warning_generic(paramValue->node, "No matching connector found for parameter %s", paramValue->name);
                continue;
            }

            if (paramValue->idx2 > input->dims[1]->end || paramValue->idx2 < input->dims[1]->start) {
                xml_warning_generic(paramValue->node, "No matching connector found for parameter %s", paramValue->name);
                continue;
            }

            index = (paramValue->idx1 - input->dims[0]->start) * (input->dims[1]->end - input->dims[1]->start + 1) + paramValue->idx2 - input->dims[1]->start;
        }

        switch (input->type->con) {
            case CHANNEL_DOUBLE:
                retVal = xml_attr_double(paramTypeNode, "value", (double*)input->values + index, SSD_MANDATORY);
                break;
            case CHANNEL_INTEGER:
                retVal = xml_attr_int(paramTypeNode, "value", (int*)input->values + index, SSD_MANDATORY);
                break;
            case CHANNEL_BOOL:
                retVal = xml_attr_bool(paramTypeNode, "value", (int*)input->values + index, SSD_MANDATORY);
                break;
            default:
                break;
        }

        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        filledMask[index] = 1;
    }

    // check that all parameter values were defined
    {
        size_t i = 0, sum = 0;

        for (i = 0; i < input->numValues; i++) {
            sum += filledMask[i];
        }

        if (sum != input->numValues) {
            return xml_error_generic(parameter->connectorNode, "Not all parameter values are defined");
        }
    }

    return RETURN_OK;
}

static ArrayParameterInput * SSDReadArrayParameter(SSDParameter * parameter) {
    ArrayParameterInput * input = (ArrayParameterInput *) object_create(ArrayParameterInput);
    InputElement * element = (InputElement *) input;

    size_t numParamValues = parameter->paramValues->Size(parameter->paramValues);

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) parameter->connectorNode;

    if (numParamValues == 0) {
        retVal = xml_error_generic(parameter->connectorNode, "No values defined for parameter %s", parameter->name);
        goto cleanup;
    }

    input->name = mcx_string_copy(parameter->name);
    if (!input->name) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    // read dimensions
    {
        input->numDims = 0;
        size_t i = 0;

        xmlNodePtr annotationNode = xml_annotation_of_type(parameter->connectorNode, "com.avl.model.connect.ssp.parameter");
        xmlNodePtr arrayNode = NULL;
        if (annotationNode) {
            xmlNodePtr paramNode = xml_child(annotationNode, "Parameter");
            arrayNode = xml_child(paramNode, "Array");
            if (arrayNode) {
                input->numDims = xml_num_children(arrayNode);
            }
        }

        input->dims = mcx_calloc(input->numDims, sizeof(ArrayParameterDimensionInput *));
        if (!input->dims) {
            goto cleanup;
        }

        for (i = 0; i < input->numDims; i++) {
            xmlNodePtr dimensionNode = xml_child_by_index(arrayNode, i);
            input->dims[i] = SSDReadArrayParameterDimension(dimensionNode);
            if (!input->dims[i]) {
                goto cleanup;
            }
        }
    }

    // read unit
    if (parameter->connectorUnit) {
        input->unit = mcx_string_copy(parameter->connectorUnit->name);
        if (!input->unit && parameter->connectorUnit->name) {
            retVal = RETURN_ERROR;
            goto cleanup;
        }
    }
    // read type
    {
        xmlNodePtr connectorTypeNode = xml_child_by_index(parameter->connectorNode, 0);
        char * connectorType = xml_node_get_name(connectorTypeNode);

        size_t i = 0;

        for (i = 0; _typeMappingArray[i].key; i++) {
            if (!strcmp(connectorType, _typeMappingArray[i].key)) {
                input->type = _typeMappingArray[i].value;
                break;
            }
        }

        if (!ChannelTypeIsValid(input->type)) {
            retVal = xml_error_unsupported_node(connectorTypeNode);
            goto cleanup;
        }
    }

    // read values
    {
        if (input->numDims == 0) {
            retVal = SSDReadDimensionlessArrayParameter(parameter, input);
        } else {
            retVal = SSDReadDimensionArrayParameter(parameter, input);
        }

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


static McxStatus SSDReadParameter(SSDParameter * parameter, ParameterInput * input) {
    InputElement * element = (InputElement *) input;

    element->type = INPUT_SSD;
    element->context = (void *) parameter->connectorNode;

    if (parameter->isArray) {
        input->type = PARAMETER_ARRAY;
        input->parameter.arrayParameter = SSDReadArrayParameter(parameter);

        if (!input->parameter.arrayParameter) {
            return RETURN_ERROR;
        }
    } else {
        input->type = PARAMETER_SCALAR;
        input->parameter.scalarParameter = SSDReadScalarParameter(parameter);

        if (!input->parameter.scalarParameter) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}


static ParametersInput * SSDCreateParametersInput(ObjectContainer * parameters,
                                                  const ObjectClass * parameterType,
                                                  const char * parameterAnnotationType,
                                                  fReadSSDParameterSpecificData ReadParameter,
                                                  fSSDParameterSpecificDataDefault DefaultParameter) {
    ParametersInput * input = (ParametersInput *) object_create(ParametersInput);
    InputElement * element = (InputElement *) input;

    size_t i = 0;
    size_t num = parameters->Size(parameters);

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = NULL;

    for (i = 0; i < num; i++) {
        SSDParameter * parameter = (SSDParameter*)parameters->At(parameters, i);

        // construct the parameter
        ParameterInput * parameterInput = (ParameterInput *) object_create_(parameterType);
        if (!parameterInput) {
            goto cleanup_1;
        }

        // read common parameter data
        retVal = SSDReadParameter(parameter, parameterInput);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        // read parameter specific data
        {
            if (parameterAnnotationType && ReadParameter) {
                xmlNodePtr annotationNode = xml_annotation_of_type(parameter->connectorNode, parameterAnnotationType);

                if (annotationNode) {
                    xmlNodePtr specificDataNode = xml_child(annotationNode, "SpecificData");
                    if (!specificDataNode) {
                        retVal = xml_error_missing_child(annotationNode, "SpecificData");
                        goto cleanup_1;
                    }

                    retVal = xml_validate_node(specificDataNode, parameterAnnotationType);
                    if (retVal == RETURN_ERROR) {
                        goto cleanup_1;
                    }

                    retVal = ReadParameter(specificDataNode, parameterInput);
                    if (retVal == RETURN_ERROR) {
                        goto cleanup_1;
                    }
                } else {
                    if (DefaultParameter) {
                        retVal = DefaultParameter(parameter->connectorNode, parameterInput);
                        if (retVal == RETURN_ERROR) {
                            goto cleanup_1;
                        }
                    } else {
                        retVal = xml_error_generic(parameter->connectorNode, "Annotation %s not defined", parameterAnnotationType);
                        goto cleanup_1;
                    }
                }
            }
        }

        retVal = input->parameters->PushBack(input->parameters, (Object *) parameterInput);
        if (RETURN_OK != retVal) {
            goto cleanup_1;
        }

        continue;

cleanup_1:
        retVal = RETURN_ERROR;
        object_destroy(parameterInput);
        goto cleanup_0;
    }

cleanup_0:
    if (retVal == RETURN_ERROR) {
        object_destroy(input);
        return NULL;
    }

    return input;
}


McxStatus SSDReadParameters(ComponentInput * componentInput,
                            xmlNodePtr connectorsNode,
                            xmlNodePtr paramBindingsNode,
                            fSSDParameterSetter SetParameters,
                            const ObjectClass * parameterType,
                            const char * parameterAnnotationType,
                            fReadSSDParameterSpecificData ReadParameter,
                            fSSDParameterSpecificDataDefault DefaultParameter,
                            ObjectContainer * units) {
    McxStatus retVal = RETURN_OK;
    ObjectContainer * parameters = NULL;
    ParametersInput * paramsInput = NULL;

    if (connectorsNode) {
        // collect connectors
        parameters = CollectParameterConnectors(connectorsNode, units);
        if (!parameters) {
            retVal = RETURN_ERROR;
            goto cleanup;
        }

        if (SetParameters && paramBindingsNode) {
            // collect parameter values
            retVal = CollectParameterValues(paramBindingsNode, parameters);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            paramsInput = SSDCreateParametersInput(parameters, parameterType, parameterAnnotationType, ReadParameter, DefaultParameter);
            if (!paramsInput) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            retVal = SetParameters(componentInput, paramsInput);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }
        } else if (!SetParameters) {
            // check that no parameter connectors are defined
            if (parameters->Size(parameters) > 0) {
                retVal = xml_error_generic(connectorsNode, "Parameters are not supported");
                goto cleanup;
            }
        }
    }

cleanup:
    if (parameters) {
        parameters->DestroyObjects(parameters);
        object_destroy(parameters);
    }

    if (retVal == RETURN_ERROR) {
        object_destroy(paramsInput);
    }

    return retVal;
}


McxStatus SSDSetFmuParameters(ComponentInput * compInput, ParametersInput * paramsInput) {
    McxStatus retVal = RETURN_OK;

    // construct the necessary container objects
    {
        compInput->parameters = (ParametersInput*)object_create(ParametersInput);
        compInput->initialValues = (ParametersInput*)object_create(ParametersInput);

        if (!compInput->parameters) {
            return RETURN_ERROR;
        }

        if (!compInput->initialValues) {
            return RETURN_ERROR;
        }
    }

    // set the context data
    {
        InputElement * origParams = (InputElement*)paramsInput;
        InputElement * params = (InputElement*)compInput->parameters;
        InputElement * initVals = (InputElement*)compInput->initialValues;

        params->type = origParams->type;
        params->context = origParams->context;

        initVals->type = origParams->type;
        initVals->context = origParams->context;
    }

    // re-organize the read parameters
    {
        size_t i = 0;
        size_t numParameters = paramsInput->parameters->Size(paramsInput->parameters);

        for (i = 0; i < numParameters; i++) {
            FmuParameterInput * paramInput = (FmuParameterInput*)paramsInput->parameters->At(paramsInput->parameters, i);
            ParametersInput * newContainer = paramInput->isInitialValue ? compInput->initialValues : compInput->parameters;

            retVal = newContainer->parameters->PushBack(newContainer->parameters, (Object *) paramInput);
            if (retVal == RETURN_ERROR) {
                return RETURN_ERROR;
            }

            retVal = paramsInput->parameters->SetAt(paramsInput->parameters, i, NULL);
            if (retVal == RETURN_ERROR) {
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

McxStatus SSDReadFmuParameterData(xmlNodePtr specificDataNode, ParameterInput * paramInput) {
    FmuParameterInput * input = (FmuParameterInput *) paramInput;
    McxStatus retVal = RETURN_OK;

    retVal = xml_attr_bool(specificDataNode, "isInitialValue", &input->isInitialValue, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus SSDDefaultFmuParameterData(xmlNodePtr connectorNode, ParameterInput * paramInput) {
    FmuParameterInput * input = (FmuParameterInput *) paramInput;

    input->isInitialValue = FALSE;

    return RETURN_OK;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */