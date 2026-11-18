/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "util/signals.h"

#include "CentralParts.h"

#include <signal.h>


/* Thread local variable to store the name of the element which is
 * running inside the signal-handled block. */
static __thread const char * _signalThreadName = NULL;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void sigHandlerParam(int param) {
    if (_signalThreadName) {
        mcx_log(LOG_ERROR, "The element %s caused an unrecoverable error. Shutting down.", _signalThreadName);
    } else {
        mcx_log(LOG_ERROR, "An element caused an unrecoverable error. Shutting down.");
    }
    exit(1);
}


static void enableSigHandlerInterrupt() {
    static struct sigaction sigHandlerINT;

    sigHandlerINT.sa_handler = mcx_signal_handler_sigint;
    sigaction(SIGINT, &sigHandlerINT, NULL);
}

void mcx_signal_handler_set_name(const char * threadName) {
    _signalThreadName = threadName;
}

void mcx_signal_handler_unset_name(void) {
    _signalThreadName = NULL;
}

void mcx_signal_handler_enable(void) {
    static struct sigaction sigHandlerSEGV;

    _signalThreadName = NULL;

    sigHandlerSEGV.sa_handler = sigHandlerParam;
    sigemptyset(&sigHandlerSEGV.sa_mask);
    sigHandlerSEGV.sa_flags = 0;
    sigaction(SIGSEGV, &sigHandlerSEGV, NULL);

    enableSigHandlerInterrupt();
}

void mcx_signal_handler_disable(void) {
    static struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = NULL;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGSEGV, &sigIntHandler, NULL);
    _signalThreadName = NULL;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */