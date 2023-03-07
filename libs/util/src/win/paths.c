/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#define _WINSOCKAPI_    // stops windows.h including winsock.h
#include <windows.h>

#undef OS_WINDOWS // gets redefined in Shlwapi.h
#include <Shlwapi.h>

#include "common/memory.h"
#include "common/logging.h"

#include "util/paths.h"
#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char * mcx_path_get_absolute(const char * path) {
    DWORD retVal = 0;
    DWORD len = 4096;
    wchar_t * wPath = NULL;
    wchar_t * wAbsPath = NULL;

    if (NULL == path) {
        return NULL;
    }

    if (mcx_path_is_absolute(path)) {
        return mcx_string_copy(path);
    }

    wPath = mcx_string_to_widechar(path);
    wAbsPath = (wchar_t *) mcx_malloc(len * sizeof(wchar_t));

    retVal = GetFullPathNameW(wPath, len, wAbsPath, NULL);
    mcx_free(wPath);
    if (0 == retVal) {
        mcx_free(wAbsPath);
        return NULL;
    } else {
        char * absPath = mcx_string_to_utf8(wAbsPath);
        mcx_free(wAbsPath);
        return absPath;
    }
}

int mcx_path_is_absolute(const char * path) {
    if (strlen(path) > 0) {
        if (path[0] == '/' || path[0] == '\\') {
            return 1;
        }
    }
    if (strlen(path) > 1) {
        if (path[1] == ':') {
            return 1;
        }
    }

    return 0;
}

char * mcx_path_from_uri(const char * uri) {
    wchar_t * wchar_uri = NULL;
    wchar_t wchar_path[4096] = { 0 };
    DWORD buffer_len = sizeof(wchar_path) / sizeof(wchar_path[0]);
    char * path = NULL;
    HRESULT hres = S_OK;

    wchar_uri = mcx_string_to_widechar(uri);
    if (wchar_uri == NULL) {
        mcx_log(LOG_ERROR, "Cannot convert UTF-8 string to wide string");
        return NULL;
    }

    hres = PathCreateFromUrlW(wchar_uri, wchar_path, &buffer_len, 0);
    mcx_free(wchar_uri);
    if (hres != S_OK) {
        mcx_log(LOG_ERROR, "PathCreateFromUrlW returned with HRESULT error: %d", hres);
        return NULL;
    }

    path = mcx_string_to_utf8(wchar_path);
    if (path == NULL) {
        mcx_log(LOG_ERROR, "Cannot convert wide string to UTF-8");
        return NULL;
    }

    return path;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */