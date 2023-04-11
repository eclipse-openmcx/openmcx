/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_OS_H
#define MCX_UTIL_OS_H

#include <stdio.h>

#include "common/status.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Retrieves the command line arguments. argList will be NULL if they
 * could not be retrieved. On Linux this is always NULL.
 */
void mcx_os_get_args(char *** argList, int * nArgs);

/**
 * Returns the value of the environment variable or NULL. The returned string needs to be free'd.
 */
char * mcx_os_get_env_var(const char * name);

/**
 * Sets the environment variable `name` to `value`.
 */
McxStatus mcx_os_set_env_var(const char * name, const char * value);

/**
 * Replaces all occurrences of environment variables of the form
 * %VAR%, $(VAR) or ${VAR} in path with their respective values and
 * returns the result.
 */
char * mcx_resolve_env_var(const char * path);

int mcx_parse_win_env_var(const char * path, char ** res);
int mcx_parse_linux_par_env_var(const char * path, char ** res);
int mcx_parse_linux_bra_env_var(const char * path, char ** res);

/**
 * Get string description of errno.
 */
const char * mcx_os_get_errno_descr(int errnum);

/**
 * Returns a value != 0 if path exists, otherwise 0.
 */
int mcx_os_path_exists(const char * path);

/**
 * Returns the normalized path of an existing path, or NULL.
 */
char * mcx_os_path_normalize(const char * path);

/**
 * Recursively deletes a directory.
 */
McxStatus mcx_os_remove_dir_tree(const char * dir);

/**
 * @return current directory or NULL in case of an error
 */
char * mcx_os_get_current_dir(void);

char * mcx_os_get_exe_dir(void);
char * mcx_os_get_dll_path_in_exe_dir(const char * dllName);

int mcx_os_sleep_ms(unsigned int ms);

/**
 * Creates and executes a new process
 * @param args is a NULL-terminated array of arguments where args[0]
 * is the program name.
 * @return the PID of the created process or 0 if an error occurred
 */
size_t mcx_os_process_create(char * args[]);


int mcx_os_mkdir(const char * dir);

/**
 * Creates a new directory recursively.
 * @param path of new directory
 * @return success value
 */
McxStatus mcx_os_mkdir_recursive(const char * path);


/**
 * Wrapper for fopen which converts path to the system-specific unicode representation
 */
FILE * mcx_os_fopen(const char * path, const char * mode);

/**
 * Wrapper for fclose. Added for consistency reasons to mcx_os_fopen.
 */
int mcx_os_fclose(FILE * file);

/**
 * Wrapper for fprintf. Added for consistency reasons to mcx_os_fopen.
 */
int mcx_os_fprintf(FILE *stream, const char *format, ...);

#if defined (OS_WINDOWS)
char * mcx_os_win_get_last_error();
#endif // OS_WINDOWS

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_UTIL_OS_H