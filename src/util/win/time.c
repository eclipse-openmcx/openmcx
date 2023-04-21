/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "util/time.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

long mcx_time_get_clock() {
    return clock() * 1000/CLOCKS_PER_SEC;
}

void mcx_time_get(McxTime * time) {
    QueryPerformanceCounter(time); // Never fails on Win XP and up
}

void mcx_cpu_time_get(McxTime * time) {
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(GetCurrentProcess(),
                        &creation_time, &exit_time, &kernel_time, &user_time) != 0) {
        time->LowPart = (DWORD) user_time.dwLowDateTime;
        time->HighPart = (LONG) user_time.dwHighDateTime;
    } else {
        time->LowPart = (DWORD) 0;
        time->HighPart = (LONG) 0;
    }
}

void mcx_time_add(McxTime * a, McxTime * b, McxTime * result) {
    if (!(a && b && result)) {
        return;
    }
    result->QuadPart = a->QuadPart + b->QuadPart;
}

void mcx_time_diff(McxTime * start, McxTime * end, McxTime * result) {
    if (!(start && end && result)) {
        return;
    }
    result->QuadPart = end->QuadPart - start->QuadPart;
}

double mcx_time_to_micro_s(McxTime * time) {
    McxTime freq;
    McxTime micro_secs;

    QueryPerformanceFrequency(&freq); // Never fails on Win XP and up
    micro_secs.QuadPart = time->QuadPart * 1000000;
    micro_secs.QuadPart /= freq.QuadPart;

    return (double) micro_secs.QuadPart;
}

McxTime mcx_seconds_to_time(int seconds) {
    McxTime freq;
    McxTime time;

    QueryPerformanceFrequency(&freq); // Never fails on Win XP and up

    time.QuadPart = seconds * freq.QuadPart;

    return time;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */