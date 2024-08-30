/********************************************************************************
 * Copyright (c) 2023 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Components.h"

#include "reader/EnumMapping.h"

#include "reader/ssp/Ports.h"
#include "reader/ssp/Schema.h"
#include "reader/ssp/SpecificData.h"
#include "reader/ssp/Connections.h"
#include "reader/ssp/Parameters.h"

#include "reader/model/connections/ConnectionInput.h"

#include "reader/model/parameters/specific_data/FmuParameterInput.h"

#include "reader/model/components/specific_data/FmuInput.h"
#include "reader/model/components/specific_data/IntegratorInput.h"
#include "reader/model/components/specific_data/VectorIntegratorInput.h"
#include "reader/model/components/specific_data/ConstantInput.h"
#include "reader/model/components/specific_data/DcpInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

SSDComponentSpecificDataDefinition ssdCommonSpecificDataDefinition[] = {
    {"application/x-fmu-sharedlibrary", "com.avl.model.connect.ssp.component.fmu", object_class_of(ComponentTypeFmu), object_class_of(FmuInput), SSDReadFmuData, SSDDefaultFmuData, object_class_of(PortInput), NULL, NULL, SSDSetFmuParameters, object_class_of(FmuParameterInput), "com.avl.model.connect.ssp.parameter.fmu", SSDReadFmuParameterData, SSDDefaultFmuParameterData},
    {"application/avl-mcx-integrator", "com.avl.model.connect.ssp.component.integrator", object_class_of(ComponentTypeIntegrator), object_class_of(IntegratorInput), SSDReadIntegratorData, SSDDefaultData, object_class_of(PortInput), NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {"application/avl-mcx-vector-integrator", "com.avl.model.connect.ssp.component.vector_integrator", object_class_of(ComponentTypeVectorIntegrator), object_class_of(VectorIntegratorInput), SSDReadVectorIntegratorData, SSDDefaultData, object_class_of(PortInput), NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {"application/avl-mcx-constant", "com.avl.model.connect.ssp.component.constant", object_class_of(ComponentTypeConstant), object_class_of(ConstantInput), SSDReadConstantData, SSDDefaultData, object_class_of(PortInput), NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {"application/avl-mcx-dcp", "com.avl.model.connect.ssp.component.dcp", object_class_of(ComponentTypeDcp), object_class_of(DcpInput), SSDReadDcpData, NULL, object_class_of(PortInput), NULL, NULL, SSDSetDcpParameters, object_class_of(ParameterInput), NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

SSDComponentSpecificDataDefinition * ssdMcxExeComponents[] = {
    ssdCommonSpecificDataDefinition,
    NULL
};

static ComponentResultsInput * SSDReadComponentResults(xmlNodePtr annotationNode) {
    ComponentResultsInput * resultsInput = (ComponentResultsInput *) object_create(ComponentResultsInput);
    InputElement * element = (InputElement *) resultsInput;

    McxStatus retVal = RETURN_OK;

    if (!resultsInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) annotationNode;

    {
        xmlNodePtr resultsNode = xml_child(annotationNode, "Results");

        if (!resultsNode) {
            retVal = xml_error_missing_child(annotationNode, "Results");
            goto cleanup;
        }

        retVal = xml_validate_node(resultsNode, "com.avl.model.connect.ssp.component.results");
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        retVal = xml_opt_attr_enum(resultsNode, "resultLevel", storeLevelMapping, (OPTIONAL_VALUE(int) *) &resultsInput->resultLevel);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        retVal = xml_opt_attr_bool32(resultsNode, "rtFactor", &resultsInput->rtFactor);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        retVal = xml_opt_attr_double(resultsNode, "startTime", &resultsInput->startTime);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        retVal = xml_opt_attr_double(resultsNode, "endTime", &resultsInput->endTime);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        retVal = xml_opt_attr_double(resultsNode, "stepTime", &resultsInput->stepTime);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        retVal = xml_opt_attr_size_t(resultsNode, "stepCount", &resultsInput->stepCount);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(resultsInput);
        return NULL;
    }

    return resultsInput;
}

static ComponentInput * SSDReadComponent(xmlNodePtr componentNode, SSDComponentSpecificDataDefinition * components[], ObjectContainer * units) {
    ComponentInput * componentInput = NULL;
    SSDComponentSpecificDataDefinition * typeDefinition = NULL;

    McxStatus retVal = RETURN_OK;

    {
        // Type of Component
        retVal = xml_attr_enum_weak_ptr(componentNode, "type", (void **) &components[0], sizeof(SSDComponentSpecificDataDefinition), (void*)&typeDefinition, SSD_OPTIONAL);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        } else if (retVal == RETURN_WARNING) {
            // in case the type attribute is not present, SSP defines `application/x-fmu-sharedlibrary` as default
            size_t i = 0;

            for (i = 0; components[i]->mimeType != NULL; i++) {
                if (!strcmp(components[i]->mimeType, "application/x-fmu-sharedlibrary")) {
                    typeDefinition = components[i];
                    break;
                }
            }

            if (!typeDefinition) {
                retVal = xml_error_generic(componentNode, "Unsupported component type application/x-fmu-sharedlibrary");
                goto cleanup;
            }
        }

        componentInput = (ComponentInput *) object_create_(typeDefinition->inputType);
        if (!componentInput) {
            retVal = RETURN_ERROR;
            goto cleanup;
        }

        componentInput->type = (ComponentType *) object_create_(typeDefinition->compType);
        if (!componentInput->type) {
            retVal = RETURN_ERROR;
            goto cleanup;
        }
    }

    {
        InputElement * element = (InputElement *) componentInput;
        element->type = INPUT_SSD;
        element->context = (void *) componentNode;
    }

    {
        retVal = xml_attr_string(componentNode, "name", &componentInput->name, SSD_MANDATORY);
        if (RETURN_ERROR == retVal) {
            goto cleanup;
        }
    }

    {
        xmlNodePtr annotationNode = xml_annotation_of_type(componentNode, "com.avl.model.connect.ssp.component");

        if (annotationNode) {
            xmlNodePtr componentAnnotationNode = xml_child(annotationNode, "Component");

            if (!componentAnnotationNode) {
                retVal = xml_error_missing_child(annotationNode, "Component");
                goto cleanup;
            }

            retVal = xml_validate_node(componentAnnotationNode, "com.avl.model.connect.ssp.component");
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_bool32(componentAnnotationNode, "inputAtEndTime", &componentInput->inputAtEndTime);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }

            retVal = xml_opt_attr_int32(componentAnnotationNode, "triggerSequence", &componentInput->triggerSequence);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }
            retVal = xml_opt_attr_double(componentAnnotationNode, "deltaTime", &componentInput->deltaTime);
            if (retVal == RETURN_ERROR) {
                goto cleanup;
            }
        }
    }

    {
        // read ports:
        //
        // <Connectors> contain both inputs and outputs. They are
        // <Connector> nodes, discerned by their kind attribute which
        // is "input", "output", "parameter", etc.
        xmlNodePtr connectorsNode = xml_child(componentNode, "Connectors");
        if (connectorsNode) {
            componentInput->inports = SSDReadComponentPorts(connectorsNode,
                                                            "input",
                                                            typeDefinition->portType,
                                                            typeDefinition->portAnnotationType,
                                                            typeDefinition->ReadPort,
                                                            units);
            if (!componentInput->inports) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }

            componentInput->outports = SSDReadComponentPorts(connectorsNode,
                                                             "output",
                                                             typeDefinition->portType,
                                                             typeDefinition->portAnnotationType,
                                                             typeDefinition->ReadPort,
                                                             units);
            if (!componentInput->outports) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }
        } else {
            componentInput->inports = NULL;
            componentInput->outports = NULL;
        }
    }

    {
        // read parameters
        xmlNodePtr connectorsNode = xml_child(componentNode, "Connectors");
        xmlNodePtr paramBindingsNode = xml_child(componentNode, "ParameterBindings");

        retVal = SSDReadParameters(componentInput,
                                   connectorsNode,
                                   paramBindingsNode,
                                   typeDefinition->SetParameters,
                                   typeDefinition->parameterType,
                                   typeDefinition->parameterAnnotationType,
                                   typeDefinition->ReadParameter,
                                   typeDefinition->DefaultParameter,
                                   units);
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }
    }

    {
        // read specific data
        if (typeDefinition->annotationType) {
            xmlNodePtr annotationNode = xml_annotation_of_type(componentNode, typeDefinition->annotationType);

            if (typeDefinition->Read) {
                if (annotationNode) {
                    xmlNodePtr specificDataNode = xml_child(annotationNode, "SpecificData");
                    if (!specificDataNode) {
                        retVal = xml_error_missing_child(annotationNode, "SpecificData");
                        goto cleanup;
                    }

                    retVal = xml_validate_node(specificDataNode, typeDefinition->annotationType);
                    if (retVal == RETURN_ERROR) {
                        goto cleanup;
                    }

                    retVal = typeDefinition->Read(componentNode, specificDataNode, componentInput);
                    if (retVal == RETURN_ERROR) {
                        goto cleanup;
                    }
                } else {
                    if (typeDefinition->DefaultSpecificData) {
                        retVal = typeDefinition->DefaultSpecificData(componentNode, componentInput);
                        if (retVal == RETURN_ERROR) {
                            goto cleanup;
                        }
                    } else {
                        retVal = xml_error_generic(componentNode, "Annotation %s not defined", typeDefinition->annotationType);
                        goto cleanup;
                    }
                }
            } else if (annotationNode) {
                retVal = xml_error_unsupported_node(annotationNode);
                goto cleanup;
            }
        } else {
            if (typeDefinition->DefaultSpecificData) {
                retVal = typeDefinition->DefaultSpecificData(componentNode, componentInput);
                if (retVal == RETURN_ERROR) {
                    goto cleanup;
                }
            }
        }
    }

    {
        // read results
        xmlNodePtr annotationNode = xml_annotation_of_type(componentNode, "com.avl.model.connect.ssp.component.results");

        if (annotationNode) {
            componentInput->results = SSDReadComponentResults(annotationNode);
            if (!componentInput->results) {
                goto cleanup;
            }
        } else {
            componentInput->results = NULL;
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(componentInput);
        return NULL;
    }

    return componentInput;
}

static MapStringInt ssdImplementationMapping[] = {
    {"any", SSD_IMPLEMENTATION_ANY},
    {"CoSimulation", SSD_IMPLEMENTATION_COSIM},
    {"ModelExchange", SSD_IMPLEMENTATION_ME},
    {NULL, 0}
};

ComponentsInput * SSDReadComponents(xmlNodePtr componentsNode,
                                    SSDComponentSpecificDataDefinition * components[],
                                    int processSubsystems,
                                    SSDImplementation supportedImplementation,
                                    ObjectContainer * units) {
    ComponentsInput * componentsInput = (ComponentsInput *) object_create(ComponentsInput);
    InputElement * element = (InputElement *) componentsInput;

    McxStatus retVal = RETURN_OK;

    if (!componentsInput) {
        return NULL;
    }

    element->type = INPUT_SSD;
    element->context = (void *) componentsNode;

    {
        size_t num = xml_num_children(componentsNode);
        size_t i = 0;

        for (i = 0; i < num; i++) {
            xmlNodePtr componentNode = xml_child_by_index(componentsNode, i);
            ComponentInput * component = NULL;

            if (!strcmp(xml_node_get_name(componentNode), "Component")) {
                SSDImplementation implementation = SSD_IMPLEMENTATION_ANY;
                retVal = xml_attr_enum(componentNode, "implementation", ssdImplementationMapping, (int*)&implementation, SSD_OPTIONAL);
                if (retVal == RETURN_ERROR) {
                    goto cleanup_1;
                }

                // check that the correct implementation type is chosen
                if (implementation != SSD_IMPLEMENTATION_ANY && implementation != supportedImplementation) {
                    retVal = xml_error_generic(componentNode, "Implementation type not allowed");
                    goto cleanup_1;
                }

                component = SSDReadComponent(componentNode, components, units);
                if (!component) {
                    goto cleanup_1;
                }
            } else {
                xml_error_unsupported_node(componentNode);
                goto cleanup_1;
            }

            retVal = componentsInput->components->PushBack(componentsInput->components, (Object *) component);
            if (RETURN_ERROR == retVal) {
                goto cleanup_1;
            }

            continue;

        cleanup_1:
            retVal = RETURN_ERROR;
            object_destroy(component);
            goto cleanup_0;
        }
    }

cleanup_0:
    if (retVal == RETURN_ERROR) {
        object_destroy(componentsInput);
        return NULL;
    }

    return componentsInput;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */