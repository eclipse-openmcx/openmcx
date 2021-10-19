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
static __thread const char * _signalFunctionName = NULL;
static __thread const char * _signalFunctionNameStack1 = NULL;
static __thread const char * _signalFunctionNameStack2 = NULL;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void sigHandlerParam(int param) {
    if (_signalThreadName) {
        if (_signalFunctionName) {
            mcx_log(LOG_ERROR, "The element %s caused an unrecoverable error in %s. Shutting down.", _signalThreadName, _signalFunctionName);
        } else {
            mcx_log(LOG_ERROR, "The element %s caused an unrecoverable error. Shutting down.", _signalThreadName);
        }
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

void mcx_signal_handler_set_function_internal(const char * functionName) {
    if (_signalFunctionNameStack2 != NULL) {
        mcx_log(LOG_ERROR, "Signal handler function callstack overflow!");
        exit(1); // I guess there is a better way to handle this
    }
    if (_signalFunctionNameStack1 != NULL) {
        _signalFunctionNameStack2 = _signalFunctionNameStack1;
    }
    if (_signalFunctionName != NULL) {
        _signalFunctionNameStack1 = _signalFunctionName;
    }
    _signalFunctionName = functionName;
}

void mcx_signal_handler_unset_function(void) {
    if (_signalFunctionName == NULL) {
        mcx_log(LOG_WARNING, "Signal handler function callstack empty. Cannot pop non-existing element.");
    }
    if (_signalFunctionNameStack1 != NULL) {
        _signalFunctionName = _signalFunctionNameStack1;
        if (_signalFunctionNameStack2 != NULL) {
            _signalFunctionNameStack1 = _signalFunctionNameStack2;
        }
    } else {
        _signalFunctionName = NULL;
    }
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

const char * mcx_signal_handler_get_function_name(void) {
    return _signalFunctionName;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */