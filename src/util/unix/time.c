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
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

long mcx_time_get_clock() {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) == 0) {
        return (int) (time.tv_sec*1000. + time.tv_nsec/1000.);
    } else {
        return 0;
    }
}

void mcx_time_get(McxTime * time) {
    struct timespec time_;
    if (clock_gettime(CLOCK_MONOTONIC, &time_) == 0) {
        time->tv_sec = (time_t) time_.tv_sec;
        time->tv_usec = (suseconds_t) (time_.tv_nsec / 1000);
    } else {
        time->tv_sec = (time_t) 0;
        time->tv_usec = (suseconds_t) 0;
    }
}

void mcx_cpu_time_get(McxTime * time) {
    clock_t time_clock_cycles = clock();
    if (time_clock_cycles != (clock_t) -1) {
        clock_t time_sec = time_clock_cycles / CLOCKS_PER_SEC;
        clock_t time_usec = time_clock_cycles % CLOCKS_PER_SEC;
        time->tv_sec = (time_t) time_sec;
        time->tv_usec = (suseconds_t) time_usec;
    } else {
        time->tv_sec = (time_t) 0;
        time->tv_usec = (suseconds_t) 0;
    }
}

void mcx_time_add(McxTime * a, McxTime * b, McxTime * result) {
    if (!(a && b && result)) {
        return;
    }

    timeradd(a, b, result);
}

void mcx_time_diff(McxTime * start, McxTime * end, McxTime * result) {
    if (!(start && end && result)) {
        return;
    }

    timersub(end, start, result);
}

double mcx_time_to_seconds(McxTime * time) {
    return time->tv_sec + time->tv_usec / 1000000.0;
}

McxTime mcx_seconds_to_time(int seconds) {
    McxTime time;
    time.tv_sec = seconds;
    time.tv_usec = 0;

    return time;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */