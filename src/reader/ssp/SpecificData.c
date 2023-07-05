/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/SpecificData.h"

#include "reader/EnumMapping.h"

#include "reader/model/components/specific_data/FmuInput.h"
#include "reader/model/components/specific_data/IntegratorInput.h"
#include "reader/model/components/specific_data/VectorIntegratorInput.h"
#include "reader/model/components/specific_data/ConstantInput.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

McxStatus SSDCheckSourceEmpty(xmlNodePtr componentNode) {
    McxStatus retVal = RETURN_OK;
    char * source = NULL;

    retVal = xml_attr_string(componentNode, "source", &source, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    if (strlen(source) != 0) {
        retVal = xml_error_generic(componentNode, "Attribute source must be empty");
        goto cleanup;
    }

cleanup:
    if (source) { mcx_free(source); }

    return retVal;
}

McxStatus SSDDefaultData(xmlNodePtr componentNode, ComponentInput * compInput) {
    return SSDCheckSourceEmpty(componentNode);
}

McxStatus SSDDefaultFmuData(xmlNodePtr componentNode, ComponentInput * compInput) {
    FmuInput * input = (FmuInput *) compInput;
    McxStatus retVal = RETURN_OK;

    retVal = xml_attr_path(componentNode, "source", &input->fmuFile, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus SSDReadFmuData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput) {
    FmuInput * input = (FmuInput *) compInput;
    McxStatus retVal = RETURN_OK;

    {
        retVal = xml_attr_path(componentNode, "source",
                               &input->fmuFile,
                               SSD_MANDATORY);
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
        }
    }

    retVal = xml_opt_attr_bool32(specificDataNode, "fmuLogging", &input->isLogging);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    {
        xmlNodePtr logCategoriesNode = xml_child(specificDataNode, "LogCategories");
        if (logCategoriesNode) {
            size_t i = 0;

            input->numLogCategories = xml_num_children(logCategoriesNode);
            input->logCategories = mcx_calloc(input->numLogCategories, sizeof(char *));
            if (!input->logCategories) {
                return RETURN_ERROR;
            }

            for (i = 0; i < input->numLogCategories; i++) {
                xmlNodePtr logCategoryNode = xml_child_by_index(logCategoriesNode, i);

                retVal = xml_attr_string(logCategoryNode, "value", &input->logCategories[i], SSD_MANDATORY);
                if (RETURN_OK != retVal) {
                    return RETURN_ERROR;
                }
            }
        }
    }

    retVal = xml_opt_attr_bool32(specificDataNode, "modelInternalVariables", &input->modelInternalVariables);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus SSDReadIntegratorData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput) {
    IntegratorInput * input = (IntegratorInput *) compInput;
    McxStatus retVal = RETURN_OK;

    retVal = SSDCheckSourceEmpty(componentNode);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    retVal = xml_opt_attr_double(specificDataNode, "gain", &input->gain);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    retVal = xml_opt_attr_int64(specificDataNode, "numSubSteps", &input->numSubSteps);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    retVal = xml_opt_attr_double(specificDataNode, "initialState", &input->initialState);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    return retVal;
}

McxStatus SSDReadVectorIntegratorData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput) {
    VectorIntegratorInput * input = (VectorIntegratorInput *) compInput;
    McxStatus retVal = RETURN_OK;

    retVal = SSDCheckSourceEmpty(componentNode);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    retVal = xml_opt_attr_double(specificDataNode, "initialState", &input->initialState);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static MapStringChannelType _scalarConstantTypeMapping[] = {
    {"Real", &ChannelTypeDouble},
    {"Integer", &ChannelTypeInteger},
    {"String", &ChannelTypeString},
    {"Boolean", &ChannelTypeBool},
    {NULL, &ChannelTypeUnknown}
};

static ScalarConstantValueInput * SSDReadScalarConstantValue(xmlNodePtr node) {
    ScalarConstantValueInput * input = (ScalarConstantValueInput *) object_create(ScalarConstantValueInput);
    InputElement * element = (InputElement *) input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) node;

    {
        size_t i = 0;

        for (i = 0; _scalarConstantTypeMapping[i].key; i++) {
            if (!strcmp(xml_node_get_name(node), _scalarConstantTypeMapping[i].key)) {
                input->type = _scalarConstantTypeMapping[i].value;
                break;
            }
        }

        if (!ChannelTypeIsValid(input->type)) {
            retVal = xml_error_unsupported_node(node);
            goto cleanup;
        }
    }

    retVal = xml_attr_channel_value_data(node, "value", input->type, &input->value, SSD_MANDATORY);
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

static MapStringChannelType _vectorConstantTypeMapping[] = {
    {"RealVector", &ChannelTypeDouble},
    {"IntegerVector", &ChannelTypeInteger},
    {NULL, &ChannelTypeUnknown}
};

static ArrayConstantValueInput * SSDReadArrayConstantValue(xmlNodePtr node) {
    ArrayConstantValueInput * input = (ArrayConstantValueInput *) object_create(ArrayConstantValueInput);
    InputElement * element = (InputElement *) input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) node;

    {
        size_t i = 0;

        for (i = 0; _vectorConstantTypeMapping[i].key; i++) {
            if (!strcmp(xml_node_get_name(node), _vectorConstantTypeMapping[i].key)) {
                input->type = _vectorConstantTypeMapping[i].value;
                break;
            }
        }

        if (!ChannelTypeIsValid(input->type)) {
            retVal = xml_error_unsupported_node(node);
            goto cleanup;
        }
    }

    switch (input->type->con) {
        case CHANNEL_DOUBLE:
            retVal = xml_attr_double_vec(node, "value", &input->numValues, (double**)&input->values, SSD_MANDATORY);
            break;
        case CHANNEL_INTEGER:
            retVal = xml_attr_int64_vec(node, "value", &input->numValues, (int64_t**)&input->values, SSD_MANDATORY);
            break;
        default:
            retVal = xml_error_unsupported_node(node);
            break;
    }

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

static ConstantValueInput * SSDReadConstantValue(xmlNodePtr valueNode) {
    ConstantValueInput * input = (ConstantValueInput *) object_create(ConstantValueInput);
    InputElement * element = (InputElement *) input;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) valueNode;

    if (!strcmp("RealVector", xml_node_get_name(valueNode)) || !strcmp("IntegerVector", xml_node_get_name(valueNode))) {
        input->type = CONSTANT_VALUE_ARRAY;
        input->value.array = SSDReadArrayConstantValue(valueNode);
        if (!input->value.array) {
            retVal = RETURN_ERROR;
            goto cleanup;
        }
    } else {
        input->type = CONSTANT_VALUE_SCALAR;
        input->value.scalar = SSDReadScalarConstantValue(valueNode);
        if (!input->value.scalar) {
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

static ConstantValuesInput * SSDReadConstantValues(xmlNodePtr specificDataNode) {
    ConstantValuesInput * input = (ConstantValuesInput *) object_create(ConstantValuesInput);
    InputElement * element = (InputElement *) input;
    size_t num = xml_num_children(specificDataNode);
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    if (!input) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) specificDataNode;

    for (i = 0; i < num; i++) {
        xmlNodePtr valueNode = xml_child_by_index(specificDataNode, i);
        ConstantValueInput * constantValue = NULL;

        constantValue = SSDReadConstantValue(valueNode);
        if (!constantValue) {
            retVal = RETURN_ERROR;
            goto cleanup_1;
        }

        retVal = input->values->PushBack(input->values, (Object *) constantValue);
        if (RETURN_ERROR == retVal) {
            goto cleanup_1;
        }

        continue;

    cleanup_1:
        object_destroy(constantValue);
        goto cleanup_0;
    }

cleanup_0:
    if (RETURN_ERROR == retVal) {
        object_destroy(input);
    }

    return input;
}

McxStatus SSDReadConstantData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput) {
    ConstantInput * input = (ConstantInput *) compInput;
    McxStatus retVal = RETURN_OK;

    retVal = SSDCheckSourceEmpty(componentNode);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    input->values = SSDReadConstantValues(specificDataNode);
    if (!input->values) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */