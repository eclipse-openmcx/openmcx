/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_SSP_SPECIFIC_DATA_H
#define MCX_READER_SSP_SPECIFIC_DATA_H

#include "reader/ssp/Util.h"
#include "reader/model/components/ComponentInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


McxStatus SSDReadFmuData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput);
McxStatus SSDReadIntegratorData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput);
McxStatus SSDReadVectorIntegratorData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput);
McxStatus SSDReadConstantData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput);

McxStatus SSDCheckSourceEmpty(xmlNodePtr componentNode);
McxStatus SSDDefaultData(xmlNodePtr componentNode, ComponentInput * compInput);
McxStatus SSDDefaultFmuData(xmlNodePtr componentNode, ComponentInput * compInput);

McxStatus SSDReadDcpData(xmlNodePtr componentNode, xmlNodePtr specificDataNode, ComponentInput * compInput);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // !MCX_READER_SSP_SPECIFIC_DATA_H