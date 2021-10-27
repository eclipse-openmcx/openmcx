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

#undef OS_WINDOWS //gets redefined in Shlwapi.h
#include "Shlwapi.h"
#include <tchar.h>
#include <strsafe.h>
#include <signal.h>

#include "CentralParts.h"

/* Thread local variable to store the name of the element which is
 * running inside the signal-handled block. */
__declspec( thread ) static const char * _signalThreadName = NULL;
__declspec( thread ) static const char * _signalFunctionName = NULL;
__declspec( thread ) static const char * _signalFunctionNameStack1 = NULL;
__declspec( thread ) static const char * _signalFunctionNameStack2 = NULL;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static LONG WINAPI HandleException(PEXCEPTION_POINTERS exception) {
    if (_signalThreadName) {
        if (_signalFunctionName) {
            mcx_log(LOG_ERROR, "The element %s caused an unrecoverable error in %s.", _signalThreadName, _signalFunctionName);
        } else {
            mcx_log(LOG_ERROR, "The element %s caused an unrecoverable error.", _signalThreadName);
        }
    } else {
        mcx_log(LOG_ERROR, "An element caused an unrecoverable error.");
    }

    switch (exception->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
        mcx_log(LOG_ERROR, "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.");
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        mcx_log(LOG_ERROR, "The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.");
        break;
    case EXCEPTION_BREAKPOINT:
        mcx_log(LOG_ERROR, "A breakpoint was encountered.");
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        mcx_log(LOG_ERROR, "The thread tried to read or write data that is misaligned on hardware that does not provide alignment.For example, 16 - bit values must be aligned on 2 - byte boundaries; 32 - bit values on 4 - byte boundaries, and so on.");
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        mcx_log(LOG_ERROR, "One of the operands in a floating - point operation is denormal.A denormal value is one that is too small to represent as a standard floating - point value.");
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        mcx_log(LOG_ERROR, "The thread tried to divide a floating - point value by a floating - point divisor of zero.");
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        mcx_log(LOG_ERROR, "The result of a floating - point operation cannot be represented exactly as a decimal fraction.");
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        mcx_log(LOG_ERROR, "This exception represents any floating - point exception not included in this list.");
        break;
    case EXCEPTION_FLT_OVERFLOW:
        mcx_log(LOG_ERROR, "The exponent of a floating - point operation is greater than the magnitude allowed by the corresponding type.");
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        mcx_log(LOG_ERROR, "The stack overflowed or underflowed as the result of a floating - point operation.");
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        mcx_log(LOG_ERROR, "The exponent of a floating - point operation is less than the magnitude allowed by the corresponding type.");
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        mcx_log(LOG_ERROR, "The thread tried to execute an invalid instruction.");
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        mcx_log(LOG_ERROR, "The thread tried to access a page that was not present, and the system was unable to load the page.For example, this exception might occur if a network connection is lost while running a program over the network.");
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        mcx_log(LOG_ERROR, "The thread tried to divide an integer value by an integer divisor of zero.");
        break;
    case EXCEPTION_INT_OVERFLOW:
        mcx_log(LOG_ERROR, "The result of an integer operation caused a carry out of the most significant bit of the result.");
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        mcx_log(LOG_ERROR, "An exception handler returned an invalid disposition to the exception dispatcher.Programmers using a high - level language such as C should never encounter this exception.");
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        mcx_log(LOG_ERROR, "The thread tried to continue execution after a noncontinuable exception occurred.");
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        mcx_log(LOG_ERROR, "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.");
        break;
    case EXCEPTION_SINGLE_STEP:
        mcx_log(LOG_ERROR, "A trace trap or other single - instruction mechanism signaled that one instruction has been executed.");
        break;
    case EXCEPTION_STACK_OVERFLOW:
        mcx_log(LOG_ERROR, "The thread used up its stack.");
        break;
    }

    mcx_log(LOG_ERROR, "Shutting down.");
    exit(1);
}

static void enableSigHandlerInterrupt() {
    signal(SIGINT, &mcx_signal_handler_sigint);
}

void mcx_signal_handler_set_name(const char * threadName) {
    _signalThreadName = threadName;
}

void mcx_signal_handler_unset_name(void) {
    _signalThreadName = NULL;
}

void mcx_signal_handler_set_function(const char * functionName) {
    if (_signalFunctionNameStack2 != NULL) {
        mcx_log(LOG_ERROR, "Signal handler function callstack overflow!");
        exit(1); // I guess there is a better way to handle this
    }
    _signalFunctionNameStack2 = _signalFunctionNameStack1;
    _signalFunctionNameStack1 = _signalFunctionName;
    _signalFunctionName = functionName;
}

void mcx_signal_handler_unset_function(void) {
    if (_signalFunctionName == NULL) {
        mcx_log(LOG_WARNING, "Signal handler function callstack in element %s empty. Cannot pop non-existing element.", _signalThreadName);
    }
    _signalFunctionName = _signalFunctionNameStack1;
    _signalFunctionNameStack1 = _signalFunctionNameStack2;
    _signalFunctionNameStack2 = NULL;
}

void mcx_signal_handler_enable(void) {
    _signalThreadName = NULL;
    SetUnhandledExceptionFilter(HandleException);

    enableSigHandlerInterrupt();
}

void mcx_signal_handler_disable(void) {
    SetUnhandledExceptionFilter(NULL);
    _signalThreadName = NULL;
}

const char * mcx_signal_handler_get_function_name(void) {
    return _signalFunctionName;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */