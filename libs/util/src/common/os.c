/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include "common/memory.h"

#include "util/os.h"
#include "util/paths.h"
#include "util/string.h"


char * mcx_os_get_dll_path_in_exe_dir(const char * dllName) {
    char * dir = mcx_os_get_exe_dir();

    const char * pathList[2];
    char * dllPath = NULL;

    if (!dir) {
        return NULL;
    }

    pathList[0] = (const char *) dir;
    pathList[1] = dllName;

    mcx_path_merge(pathList, 2, &dllPath);

    mcx_free(dir);

    return dllPath;
}

McxStatus mcx_os_mkdir_recursive(const char * path) {
    int retVal;
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s", path);

    len = strlen(tmp);
    if(tmp[len - 1] == PLATFORMPATHSIGN)
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++)
        if(*p == PLATFORMPATHSIGN) {
            *p = 0;
            retVal = mcx_os_mkdir(tmp);
            *p = PLATFORMPATHSIGN;
        }
    retVal = mcx_os_mkdir(tmp);

    if (-1 == retVal) {
        if (EEXIST == errno) {
            return RETURN_WARNING;
        } else {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

int mcx_os_fclose(FILE * file) {
    return fclose(file);
}

int mcx_os_fprintf(FILE *stream, const char *format, ...) {
    int ret;
    va_list args;

    va_start(args, format);
    ret = vfprintf(stream, format, args);
    va_end(args);

    return ret;
}

int mcx_parse_win_env_var(char * path, char ** res) {
    size_t i = 0;

    *res = NULL;

    // expect %
    if (path[i] == '%') {
        ++i;
    } else {
        return 0;
    }

    // skip over name of variable
    while (path[i] && path[i] != '%') {
        ++i;
    }

    // expect %
    if (path[i] == '%') {
        ++i;
    } else {
        return 0;
    }

    // copy name of variable
    *res = mcx_calloc(i - 2 + 1, sizeof(char));
    if (!*res) {
        return 0;
    }

    memcpy(*res, path + 1, i - 2);

    return (int)i;
}

static int input_parse_linux_env_var(char * path, char ** res, char open, char close) {
    size_t i = 0;

    *res = NULL;

    // expect $(
    if (path[i] == '$') {
        ++i;
    } else {
        return 0;
    }

    if (path[i] == open) {
        ++i;
    } else {
        return 0;
    }

    // skip over name of variable
    while (path[i] && path[i] != close) {
        ++i;
    }

    // expect )
    if (path[i] == close) {
        ++i;
    } else {
        return 0;
    }

    // copy name of variable
    *res = mcx_calloc(i - 3 + 1, sizeof(char));
    if (!*res) {
        return 0;
    }

    memcpy(*res, path + 2, i - 3);

    return (int)i;
}

int mcx_parse_linux_par_env_var(char * path, char ** res) {
    return input_parse_linux_env_var(path, res, '(', ')');
}

int mcx_parse_linux_bra_env_var(char * path, char ** res) {
    return input_parse_linux_env_var(path, res, '{', '}');
}

/**
 * Parses an environment variable in one the following forms:
 *  - %VAR%
 *  - $(VAR)
 *  - ${VAR}
 *
 * The variable has to be right at the begining of path. If found,
 * name of the variable, without the markers "%", "$()", or "${}" is
 * returned and the total number of characters of the variable is
 * returned, so path + the returned value is the first character after
 * the '%', or closing ')' or '}'.
 *
 * If path does not start with an environment variable, *res is set to
 * NULL and 0 is returned.
 */
static int mcx_parse_env_var(char * path, char ** res) {
    int num;

    if (num = mcx_parse_win_env_var(path, res)) {
        return num;
    }
    if (num = mcx_parse_linux_par_env_var(path, res)) {
        return num;
    }
    if (num = mcx_parse_linux_bra_env_var(path, res)) {
        return num;
    }

    *res = NULL;
    return 0;
}

static int mcx_parse_escaped_env_delimiter_win(char * path) {
    if (strlen(path) > 1 && path[0] == '%' && path[1] == '%') {
        return (int)'%';
    } else {
        return 0;
    }
}

static int mcx_parse_escaped_env_delimiter_linux(char * path) {
    if (strlen(path) > 1 && path[0] == '$' && path[1] == '$') {
        return (int)'$';
    } else {
        return 0;
    }
}

static int mcx_parse_escaped_env_delimiter(char * path) {
    int num;

    if (num = mcx_parse_escaped_env_delimiter_win(path)) {
        return num;
    }
    if (num = mcx_parse_escaped_env_delimiter_linux(path)) {
        return num;
    }

    return 0;
}

char * mcx_resolve_env_var(const char * path) {
    int i = 0;

    int resolved_len = 1; // for '\0'
    char * resolved = mcx_calloc(resolved_len, sizeof(char));
    if (!resolved) {
        return NULL;
    }

    while (path[i]) {
        char * name;
        char quote;
        int num;

        // skip over chars until we find a variable or a quote
        while (path[i] &&
               !(quote = mcx_parse_escaped_env_delimiter(path + i)) &&
               !(num   = mcx_parse_env_var(path + i, &name))) {
            ++i;
        }

        // copy non-env-var part
        if (i > 0) {
            resolved_len += i;
            resolved = mcx_realloc(resolved, resolved_len);
            if (!resolved) {
                if (name) { mcx_free(name); }
                return NULL;
            }

            strncat(resolved, path, i);
            resolved[resolved_len - 1] = '\0';
        }

        // copy quoted char
        if (quote) {
            resolved_len += 1;
            resolved = mcx_realloc(resolved, resolved_len);
            if (!resolved) {
                if (name) { mcx_free(name); }
                return NULL;
            }

            resolved[resolved_len - 2] = quote;
            resolved[resolved_len - 1] = '\0';

            // jump over quote-char
            ++i; ++i;
        }

        // copy env-var value
        if (name) {
            char * value;

            value = mcx_os_get_env_var(name);
            if (value) {
                resolved_len += (int)strlen(value);
                resolved = mcx_realloc(resolved, resolved_len);
                if (!resolved) {
                    mcx_free(name);
                    mcx_free(value);

                    return NULL;
                }

                strcat(resolved, value);
                mcx_free(value);
            }

            mcx_free(name);
        }

        path += i + num; i = 0;
    }

    return resolved;
}