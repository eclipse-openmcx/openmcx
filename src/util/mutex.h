/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_MUTEX_H
#define MCX_UTIL_MUTEX_H

#if(__APPLE__)
#include "sys/types.h"
#endif

#include "CentralParts.h"
#if defined (ENABLE_MT)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#if defined(OS_WINDOWS)
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#include <windows.h>

//mutex
#define McxMutex          CRITICAL_SECTION

#elif defined(OS_LINUX)

//mutex
#define McxMutex    pthread_mutex_t

#endif


void mcx_mutex_create(McxMutex *);
void mcx_mutex_destroy(McxMutex *);
void mcx_mutex_lock(McxMutex *);
void mcx_mutex_unlock(McxMutex *);
int mcx_mutex_try_lock(McxMutex * mutex);



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //ENABLE_MT

#endif // MCX_UTIL_MUTEX_H