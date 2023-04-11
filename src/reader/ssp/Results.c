/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Results.h"
#include "reader/ssp/Schema.h"

#include "reader/EnumMapping.h"
#include "reader/task/BackendInput.h"
#include "reader/task/BackendsInput.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static BackendInput * SSDReadBackend(xmlNodePtr backendNode) {
    BackendInput * backendInput = (BackendInput*)object_create(BackendInput);
    InputElement * element = (InputElement *)backendInput;

    McxStatus retVal = RETURN_OK;

    if (!backendInput) {
        return NULL;
    }

    if (!backendNode) {
        mcx_log(LOG_ERROR, "SSDReadBackend: No node provided");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    element->type = INPUT_SSD;
    element->context = (void*)backendNode;

    retVal = xml_attr_enum(backendNode, "type", backendTypeMapping, (int*)&backendInput->type, SSD_MANDATORY);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_opt_attr_bool32(backendNode, "storeAtRuntime", &backendInput->storeAtRuntime);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(backendInput);
        return NULL;
    }

    return backendInput;
}


static BackendsInput * SSDReadBackends(xmlNodePtr backendsNode) {
    BackendsInput * backendsInput = (BackendsInput*)object_create(BackendsInput);
    InputElement * element = (InputElement *)backendsInput;
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    if (!backendsInput) {
        return NULL;
    }

    if (!backendsNode) {
        mcx_log(LOG_ERROR, "SSDReadBackends: No node provided");
        retVal = RETURN_ERROR;
        goto cleanup_0;
    }

    element->type = INPUT_SSD;
    element->context = (void*)backendsNode;

    for (i = 0; i < xml_num_children(backendsNode); i++) {
        xmlNodePtr backendNode = xml_child_by_index(backendsNode, i);
        BackendInput * backend = SSDReadBackend(backendNode);

        if (!backend) {
            retVal = RETURN_ERROR;
            goto cleanup_1;
        }

        retVal = backendsInput->backends->PushBack(backendsInput->backends, (Object *) backend);
        if (RETURN_ERROR == retVal) {
            goto cleanup_1;
        }

        continue;

cleanup_1:
        object_destroy(backend);
        goto cleanup_0;
    }

cleanup_0:
    if (retVal == RETURN_ERROR) {
        object_destroy(backendsInput);
        return NULL;
    }

    return backendsInput;
}


ResultsInput * SSDReadResults(xmlNodePtr annotationNode) {
    ResultsInput * resultsInput = NULL;
    InputElement * element = NULL;
    xmlNodePtr resultsNode = NULL;
    McxStatus retVal = RETURN_OK;

    if (!annotationNode) {
        resultsInput = CreateDefaultResultsInput(INPUT_SSD);
        if (!resultsInput) {
            return NULL;
        }
        goto cleanup;
    }

    resultsNode = xml_child(annotationNode, "Results");
    if (!resultsNode) {
        retVal = xml_error_missing_child(annotationNode, "Results");
        goto cleanup;
    }

    retVal = xml_validate_node(resultsNode, "com.avl.model.connect.ssp.results");
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    resultsInput = (ResultsInput*)object_create(ResultsInput);
    element = (InputElement*)resultsInput;

    if (!resultsInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void*)resultsNode;

    retVal = xml_attr_string(resultsNode, "outputDirectory", &resultsInput->outputDirectory, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_opt_attr_enum(resultsNode, "resultLevel", storeLevelMapping, (OPTIONAL_VALUE(int) *) &resultsInput->resultLevel);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    {
        xmlNodePtr backendsNode = xml_child(resultsNode, "Backends");
        if (backendsNode) {
            resultsInput->backends = SSDReadBackends(backendsNode);
            if (!resultsInput->backends) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(resultsInput);
        return NULL;
    }

    return resultsInput;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */