/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_THREADS_H
#define MCX_UTIL_THREADS_H

#include "CentralParts.h"

#if defined (ENABLE_MT)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#if(__APPLE__)
#include <sys/types.h>
#endif

#if defined(OS_WINDOWS)
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#include <windows.h>

//thread
#define McxThread         HANDLE
#define McxThreadStartRoutine   LPTHREAD_START_ROUTINE
#define McxThreadParameter      LPVOID
#define McxThreadReturn  int WINAPI

#elif defined(OS_LINUX)

//thread
#define McxThread pthread_t
typedef void * (* McxThreadStartRoutine)(void *);
#define McxThreadParameter void *

#define McxThreadReturn  void *

#endif // OS_WINDOWS

/*** implementation of windows CreateEvent (auto reset) SetEvent and WaitForSingleObject (INFINITE) */
int mcx_thread_create(McxThread *, McxThreadStartRoutine, McxThreadParameter);
void mcx_thread_exit(long ret);
int mcx_thread_terminate(McxThread handle);
int mcx_thread_join(McxThread handle, long * ret);
int mcx_thread_join_with_timeout(McxThread handle, long * ret, int secs);

int mcx_run_with_timeout(McxThreadStartRoutine fn, McxThreadParameter arg, long * retVal, int timeoutSec);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // ENABLE_MT

#endif // MCX_UTIL_THREADS_H