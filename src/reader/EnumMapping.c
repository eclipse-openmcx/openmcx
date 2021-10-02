/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/EnumMapping.h"

#include "reader/task/BackendInput.h"
#include "reader/task/TaskInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


MapStringInt storeLevelMapping[] = {
    {"none",            STORE_NONE},
    {"micro",           STORE_MICRO},
    {"coupling",        STORE_COUPLING},
    {"synchronization", STORE_SYNCHRONIZATION},
    {NULL, 0}
};

MapStringInt backendTypeMapping[] = {
    {"csv", BACKEND_CSV},
    {NULL, 0}
};

MapStringInt endTypeMapping[] = {
    {"first_component", END_TYPE_FIRST_COMPONENT},
    {"end_time", END_TYPE_TIME},
    {NULL, 0}
};

MapStringInt stepTypeMapping[] = {
    {"sequential",                STEP_TYPE_SEQUENTIAL},
    {"parallel_single_thread",    STEP_TYPE_PARALLEL_ST},
    {"parallel_one_step_size",    STEP_TYPE_PARALLEL_MT},
    {"parallel_sync_all",         STEP_TYPE_PARALLEL_MT},
    {NULL, 0}
};


MapStringInt interExtrapolationIntervalMapping[] = {
    {"coupling", INTERVAL_COUPLING},
    {"synchronization", INTERVAL_SYNCHRONIZATION},
    {NULL, 0}
};

MapStringInt interExtrapolationOrderMapping[] = {
    {"zero",   POLY_CONSTANT},
    {"first",  POLY_LINEAR},
    {NULL, 0}
};

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */