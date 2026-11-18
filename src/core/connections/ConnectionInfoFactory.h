/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_CONNECTIONINFOFACTORY_H
#define MCX_CORE_CONNECTIONS_CONNECTIONINFOFACTORY_H

#include "objects/ObjectContainer.h"
#include "reader/model/connections/ConnectionInput.h"
#include "core/Component_interface.h"
#include "objects/Vector.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


Vector * ConnectionInfoFactoryCreateConnectionInfos(ObjectContainer * components,
                                                    ConnectionInput * connInput,
                                                    Component * sourceCompOverride,
                                                    Component * targetCompOverride);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_CONNECTIONINFOFACTORY_H */