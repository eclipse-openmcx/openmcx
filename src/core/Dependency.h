/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_DEPENDENCY_H
#define MCX_CORE_DEPENDENCY_H

#include "CentralParts.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum DependencyDef {
    DEP_INDEPENDENT = 0,
    DEP_DEPENDENT = 1,
    DEP_LINEAR = 2,
    DEP_FIXED = 3,
} Dependency;

typedef struct Dependencies Dependencies;

struct Dependencies * DependenciesCreate(size_t numIn, size_t numOut);

void DependenciesDestroy(struct Dependencies * A);

McxStatus SetDependency(struct Dependencies * A, size_t inIndex, size_t outIndex, Dependency dep);

McxStatus GetDependency(struct Dependencies * A, size_t inIndex, size_t outIndex, Dependency * dep);

Dependency * GetDependencyMatrix(struct Dependencies * A);

size_t GetDependencyNumIn(struct Dependencies * A);
size_t GetDependencyNumOut(struct Dependencies * A);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_DEPENDENCY_H */