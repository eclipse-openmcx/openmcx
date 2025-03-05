/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_TIME_H
#define MCX_UTIL_TIME_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined (OS_WINDOWS)
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#include <windows.h>

typedef LARGE_INTEGER McxTime;
#elif defined (OS_LINUX)
#include <sys/time.h>

typedef struct timeval McxTime;
#endif /* OS_WINDOWS */

void mcx_time_init(McxTime * time);
void mcx_time_get(McxTime * time);
void mcx_cpu_time_get(McxTime * time);
void mcx_time_add(McxTime * a, McxTime * b, McxTime * result);
void mcx_time_diff(McxTime * start, McxTime * end, McxTime * result);
double mcx_time_to_seconds(McxTime * time);
double mcx_time_to_micro_s(McxTime * time);
double mcx_time_to_milli_s(McxTime * time);
McxTime mcx_seconds_to_time(int seconds);
long mcx_time_get_clock();


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_UTIL_TIME_H