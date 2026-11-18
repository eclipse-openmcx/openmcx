/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"

#if defined (ENABLE_MT)

#include "util/time.h"
#include "util/threads.h"
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


int mcx_thread_create(McxThread * handle, McxThreadStartRoutine start_routine, McxThreadParameter arg)
{
    if (pthread_create(handle, NULL, start_routine, arg)) {
        mcx_log(LOG_ERROR, "Error creating thread");
        return 1;
    }

    return 0;
}

int mcx_thread_terminate(McxThread handle) {
    return pthread_cancel(handle);
}

void mcx_thread_exit(long ret) {
    pthread_exit((void *) ret);
}

int mcx_thread_join(McxThread handle, long * ret) {
    return pthread_join(handle, (void * *) ret);
}

struct args {
    int joined;
    pthread_t td;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    void **res;
};

static void *waiter(void *ap)
{
    struct args *args = ap;
    pthread_join(args->td, args->res);
    pthread_mutex_lock(&args->mtx);
    args->joined = 1;
    pthread_mutex_unlock(&args->mtx);
    pthread_cond_signal(&args->cond);
    return 0;
}

// Portable implementation of `pthread_timedjoin_np()`. Inspired 
// and copied from https://stackoverflow.com/a/11552244. As this
// implementation is more costly than `pthread_timedjoin_np()`, 
// we only use this portable version for Apple platforms. 
int pthread_timedjoin_p(pthread_t td, void **res, struct timespec *ts)
{
    pthread_t tmp;
    int ret;
    struct args args = { .td = td, .res = res };

    pthread_mutex_init(&args.mtx, 0);
    pthread_cond_init(&args.cond, 0);
    pthread_mutex_lock(&args.mtx);

    ret = pthread_create(&tmp, 0, waiter, &args);
    if (!ret) {
        do {
            ret = pthread_cond_timedwait(&args.cond, &args.mtx, ts);
        } while (!args.joined && ret != ETIMEDOUT);
    }

    pthread_mutex_unlock(&args.mtx);

    pthread_cancel(tmp);
    pthread_join(tmp, 0);

    pthread_cond_destroy(&args.cond);
    pthread_mutex_destroy(&args.mtx);

    return args.joined ? 0 : ret;
}

int pthread_timedjoin(pthread_t handle, void **ret, struct timespec *time) 
{
#if(__APPLE__)
    return pthread_timedjoin_p(handle, ret, time);
#else
    return pthread_timedjoin_np(handle, ret, time);
#endif
}

int mcx_thread_join_with_timeout(McxThread handle, long * ret, int secs) {
    struct timespec time;
    int status = 0;

    // Should not fail as all implementations support CLOCK_REALTIME.
    clock_gettime(CLOCK_REALTIME, &time);

    time.tv_sec += secs;

    status = pthread_timedjoin(handle, (void * *) ret, &time);
    if (status) {
        const char * cause = NULL;

        if (status == EDEADLK) {
            cause = "Thread would deadlock";
        } else if (status == EINVAL) {
            cause = "Invalid thread";
        } else if (status == ESRCH) {
            cause = "No such thread";
        } else if (status == ETIMEDOUT) {
            cause = "Timeout";
        } else {
            cause = "Unknown error";
        }

        mcx_log(LOG_ERROR, "thread_join_with_timeout: %s", cause);
    }

    return status;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // ENABLE_MT
