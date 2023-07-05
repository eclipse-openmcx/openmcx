/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CENTRALPARTS_H
#define MCX_CENTRALPARTS_H

#include "common/status.h"

#if defined (OS_WINDOWS)
  #define SHARED_LIBRARY_PREFIX ""
    #define SHARED_LIBRARY_EXTENSION ".dll"
#elif defined OS_LINUX
  #define SHARED_LIBRARY_PREFIX "lib"
  #define SHARED_LIBRARY_EXTENSION ".so"
#else
  #error Unsupported Platform
#endif

#if defined(OS_LINUX)
#define _GNU_SOURCE 1 /* for qsort_r */
#define _DEFAULT_SOURCE 1 /* for timersub */
#include <sys/time.h>
#include <math.h>
#endif //OS_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "common/definitions.h"

#include "common/memory.h"

#define mcx_printf printf

#define ZERO_COMPARE     1.E-8

typedef struct MapStringInt {
    const char * key;
    int value;
} MapStringInt;

typedef struct {
    int stopIfFirstComponentFinished;
    int aComponentFinished;
    int errorOccurred;
} FinishState;

typedef enum ChannelMode {
    CHANNEL_MANDATORY = 1,
    CHANNEL_OPTIONAL = 2,
} ChannelMode;


typedef enum DecoupleTypeDef {
    DECOUPLE_NEVER    = 0x1,
    DECOUPLE_IFNEEDED = 0x2,
    DECOUPLE_ALWAYS   = 0x4
} DecoupleType;

typedef enum PolyOrderTypeDef {
      POLY_CONSTANT = 0
    , POLY_LINEAR = 1
} PolyOrderType;

typedef enum InterExtrapolatingTypeDef {
    EXTRAPOLATING = 0
    , INTERPOLATING = 1
    , INTEREXTRAPOLATING = 2
} InterExtrapolatingType;

typedef enum InterExtrapolationTypeDef {
    INTEREXTRAPOLATION_NONE = 0,
    INTEREXTRAPOLATION_POLYNOMIAL = 1
} InterExtrapolationType;

typedef enum IntervalTypeDef {
    INTERVAL_COUPLING = 0,
    INTERVAL_SYNCHRONIZATION = 1,
} IntervalType;

typedef struct InterExtrapolationParams {
    PolyOrderType interpolationOrder;
    PolyOrderType extrapolationOrder;
    IntervalType interpolationInterval;
    IntervalType extrapolationInterval;
} InterExtrapolationParams;

#define DEFAULT_NO_UNIT "-"
#define DEFAULT_TIME_UNIT "s"
#include "common/logging.h"

#include "objects/Object.h"
#include "objects/StringContainer.h"
#include "objects/ObjectContainer.h"


typedef enum {
    CHANNEL_STORE_IN       = 0,
    CHANNEL_STORE_OUT      = 1,
    CHANNEL_STORE_LOCAL    = 2,
    CHANNEL_STORE_RTFACTOR = 3,
    CHANNEL_STORE_NUM      = 4
} ChannelStoreType;

typedef enum StoreLevel {
    STORE_NONE  = 1,
    STORE_SYNCHRONIZATION = 2,
    STORE_COUPLING = 3,
    STORE_MICRO = 4,
} StoreLevel;

typedef enum {
    NAN_CHECK_NEVER = 0,
    NAN_CHECK_CONNECTED = 1,
    NAN_CHECK_ALWAYS = 2
} NaNCheckLevel;

char * CreateChannelID(const char * compName, const char * channelName);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CENTRALPARTS_H */