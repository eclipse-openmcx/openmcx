/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_SSP_PARAMETERS_H
#define MCX_READER_SSP_PARAMETERS_H

#include "reader/ssp/Util.h"

#include "reader/model/components/ComponentInput.h"
#include "reader/model/parameters/ParametersInput.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef McxStatus(*fSSDParameterSetter)(ComponentInput * compInput, ParametersInput * paramsInput);
typedef McxStatus(*fReadSSDParameterSpecificData)(xmlNodePtr specificDataNode, ParameterInput * paramInput);
typedef McxStatus(*fSSDParameterSpecificDataDefault)(xmlNodePtr connectorNode, ParameterInput * paramInput);

McxStatus SSDReadParameters(ComponentInput * componentInput,
                            xmlNodePtr connectorsNode,
                            xmlNodePtr paramBindingsNode,
                            fSSDParameterSetter SetParameters,
                            const ObjectClass * parameterType,
                            const char * parameterAnnotationType,
                            fReadSSDParameterSpecificData ReadParameter,
                            fSSDParameterSpecificDataDefault DefaultParameter,
                            ObjectContainer * units);


McxStatus SSDSetFmuParameters(ComponentInput * compInput, ParametersInput * paramsInput);


McxStatus SSDReadFmuParameterData(xmlNodePtr specificDataNode, ParameterInput * paramInput);

McxStatus SSDDefaultFmuParameterData(xmlNodePtr connectorNode, ParameterInput * paramInput);

McxStatus SSDSetDcpParameters(ComponentInput * compInput, ParametersInput * paramsInput);
McxStatus SSDReadDcpParameterData(xmlNodePtr specificDataNode, ParameterInput * paramInput);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */


#endif // !MCX_READER_SSP_PARAMETERS_H