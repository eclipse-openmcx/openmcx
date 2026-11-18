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
#include "util/mutex.h"

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void mcx_mutex_create( McxMutex* mutex )
{
    pthread_mutex_init ( mutex, NULL);
}

void mcx_mutex_destroy( McxMutex* mutex )
{
    pthread_mutex_destroy ( mutex );
}

void mcx_mutex_lock( McxMutex * mutex )
{
    pthread_mutex_lock( mutex );
}


int mcx_mutex_try_lock( McxMutex * mutex )
{
    return pthread_mutex_trylock( mutex );
}


void mcx_mutex_unlock( McxMutex * mutex )
{
    pthread_mutex_unlock( mutex );
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // ENABLE_MT