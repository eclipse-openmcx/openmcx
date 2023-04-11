/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Task.h"

#include "reader/EnumMapping.h"
#include "reader/ssp/Results.h"
#include "reader/ssp/Schema.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static McxStatus DefaultTask(TaskInput ** input, int defaultResults) {
    TaskInput * taskInput = CreateDefaultTaskInput(INPUT_SSD);

    if (!taskInput) {
        goto cleanup;
    }

    if (defaultResults) {
        taskInput->results = CreateDefaultResultsInput(INPUT_SSD);
        if (!taskInput->results) {
            goto cleanup;
        }
    }

    *input = taskInput;
    return RETURN_OK;

cleanup:
    object_destroy(taskInput);
    return RETURN_ERROR;
}


TaskInput * SSDReadTask(xmlNodePtr defaultExperimentNode) {
    TaskInput * taskInput = NULL;
    InputElement * element = NULL;
    McxStatus retVal = RETURN_OK;

    if (!defaultExperimentNode) {
        retVal = DefaultTask(&taskInput, TRUE);
        goto cleanup;
    }

    taskInput = (TaskInput*)object_create(TaskInput);
    element = (InputElement *)taskInput;

    if (!taskInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void*)defaultExperimentNode;

    // read information from the task annotation
    {
        xmlNodePtr annotationNode = xml_annotation_of_type(defaultExperimentNode, "com.avl.model.connect.ssp.task");

        if (!annotationNode) {
            retVal = DefaultTask(&taskInput, FALSE);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }
        } else {
            xmlNodePtr taskNode = xml_child(annotationNode, "Task");
            if (!taskNode) {
                retVal = xml_error_missing_child(annotationNode, "Task");
                goto cleanup;
            }

            retVal = xml_validate_node(taskNode, "com.avl.model.connect.ssp.task");
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_enum(taskNode, "endType", endTypeMapping, (OPTIONAL_VALUE(int) *) &taskInput->endType);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_double(taskNode, "deltaTime", &taskInput->deltaTime);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_bool32(taskNode, "sumTime", &taskInput->sumTime);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_attr_enum(taskNode, "stepType", stepTypeMapping, (int*)&taskInput->stepType, SSD_MANDATORY);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_bool32(taskNode, "inputAtEndTime", &taskInput->inputAtEndTime);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_double(taskNode, "relativeEps", &taskInput->relativeEps);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_bool32(taskNode, "timingOutput", &taskInput->timingOutput);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }
        }
    }

    retVal = xml_opt_attr_double(defaultExperimentNode, "startTime", &taskInput->startTime);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_opt_attr_double(defaultExperimentNode, "stopTime", &taskInput->endTime);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    // read information from the results annotation
    {
        xmlNodePtr annotationNode = xml_annotation_of_type(defaultExperimentNode, "com.avl.model.connect.ssp.results");
        taskInput->results = SSDReadResults(annotationNode);
        if (!taskInput->results) {
            retVal = RETURN_ERROR;
            goto cleanup;
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(taskInput);
        return NULL;
    }

    return taskInput;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */