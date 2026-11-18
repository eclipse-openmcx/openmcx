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

#include <time.h>
#include <pthread.h>

#include "util/events.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


// implementation of windows CreateEvent (auto reset) SetEvent and WaitForSingleObject (INFINITE)
int mcx_event_create(McxEvent * evnt)
{
    evnt->signalled = 0;
    if ( pthread_mutex_init(&evnt->mutex, NULL) || pthread_cond_init (&evnt->cond,  NULL) ) {
        mcx_log(LOG_ERROR, "Error creating event");
        return 1;
    }

    return 0;
}

void mcx_event_destroy(McxEvent * evnt) {
    if (pthread_mutex_destroy(&evnt->mutex)) {
        mcx_log(LOG_WARNING, "Destroying mutex of event failed");
    }

    if (pthread_cond_destroy(&evnt->cond)) {
        mcx_log(LOG_WARNING, "Destroying condition variable of event failed");
    }
}


int mcx_event_set(McxEvent *evnt)
{
    if ( pthread_mutex_lock(&evnt->mutex) ) {
        mcx_log(LOG_ERROR, "Error signalling event (mutex lock failed)");
        return 1;
    } else {
        if ( !evnt->signalled ) {
            evnt->signalled = 1;
            if ( pthread_cond_signal(&evnt->cond) ) {
                mcx_log(LOG_ERROR, "Error signalling event (pthread_cond_signal failed)");
            }
        }
        if ( pthread_mutex_unlock(&evnt->mutex) ) {
            mcx_log(LOG_ERROR, "Error signalling event (mutex unlock failed)");
            return 1;
        }
    }

    return 0;
}


int mcx_event_wait_with_timeout(McxEvent *evnt, unsigned int msTimeout)
{
    int ret = WAIT_FAILED;
    struct timespec ts;
    int rc = 0;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += (msTimeout / 1000); // 1s = 1000ms
    ts.tv_nsec += (msTimeout % 1000) * 1000000; // 1ms = 1000000ns
    if (ts.tv_nsec >= 1000000000) { // 1s = 1000000000ns
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    if ( pthread_mutex_lock( &(evnt->mutex) ) ) {
        mcx_log(LOG_ERROR, "Error waiting for event (mutex lock failed)");
    } else {
        while ( rc == 0 && !(evnt->signalled)) { //to catch spurious wakeups
            rc = pthread_cond_timedwait(&(evnt->cond), &(evnt->mutex), &ts);
        }
        if (ETIMEDOUT == rc) {
            ret = WAIT_TIMEOUT;
        } else if (evnt->signalled) {
            ret = WAIT_OBJECT_0;
        } else {
            mcx_log(LOG_ERROR, "Error waiting for event (no time-out and no signal)");
            ret = WAIT_FAILED;
        }

        evnt->signalled = 0; //resetting event
        if ( pthread_mutex_unlock( &(evnt->mutex) ) ) {
            mcx_log(LOG_ERROR, "Error waiting for event (mutex unlock failed)");
        }
    }

    return ret;
}

int mcx_event_wait(McxEvent *evnt)
{
    if ( pthread_mutex_lock( &(evnt->mutex) ) ) {
        mcx_log(LOG_ERROR, "Error waiting for event (mutex lock failed)");
        return 1;
    } else {
        while ( !(evnt->signalled) ) { //to catch spurious wakeups
            if ( pthread_cond_wait( &(evnt->cond), &(evnt->mutex) ) ) {
                mcx_log(LOG_ERROR, "Error waiting for event (pthread_cond_wait failed)");
            }
        }
        evnt->signalled = 0; //resetting event
        if ( pthread_mutex_unlock( &(evnt->mutex) ) ) {
            mcx_log(LOG_ERROR, "Error waiting for event (mutex unlock failed)");
            return 1;
        }
    }

    return 0;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // ENABLE_MT