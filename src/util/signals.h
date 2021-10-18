/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_SIGNALS_H
#define MCX_UTIL_SIGNALS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Registers a handler for SIGSEGV that exits mcx
 */
void mcx_signal_handler_enable(void);

void mcx_signal_handler_set_name(const char * threadName);
void mcx_signal_handler_unset_name(void);

void mcx_signal_handler_set_function(const char * functionName);
void mcx_signal_handler_unset_function(void);

/**
 * Deletes the handler for SIGSEGV
 */
void mcx_signal_handler_disable(void);

/**
 * Interrupt handler.
 */
void mcx_signal_handler_sigint(int param);

/**
 * Predicate whether SIGINT (Ctrl-C) was received.
 */
int mcx_signal_handler_is_interrupted(void);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_UTIL_SIGNALS_H