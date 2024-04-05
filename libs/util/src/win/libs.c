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
#undef OS_WINDOWS // gets redefinied in shlwapi.h
#include <shlwapi.h>

#include "common/logging.h"
#include "common/memory.h"

#include "util/libs.h"
#include "util/string.h"


#define LONG_PATH_PREFIX L"\\\\?\\"
#define LONG_UNC_PATH_PREFIX L"\\\\?\\UNC\\"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static void print_last_error() {
    LPVOID lpMsgBuf;
    DWORD err = GetLastError();
    switch (err) {
        case ERROR_BAD_EXE_FORMAT:
            mcx_log(LOG_ERROR, "Util: There is a mismatch in bitness (32/64) between current MCX and the dynamic library");
            break;
        default:
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                err,
                0x0409, /* language id: us english */
                (LPTSTR) &lpMsgBuf,
                0,
                NULL);
            mcx_log(LOG_ERROR, "Util: Error %d: %s", err, lpMsgBuf);
            LocalFree(lpMsgBuf);
            break;
    }
}


static wchar_t * normalize_unc_path(const wchar_t * src) {
    if (!src) {
        return NULL;
    }

    if (*src != L'\\' || *(src + 1) != L'\\') {
        return NULL; // not an UNC path
    }

    wchar_t * norm_path = (wchar_t *) mcx_calloc(wcslen(src) + 1, sizeof(wchar_t));
    if (!norm_path) {
        return NULL;
    }

    wcscpy(norm_path, src);

    // replace forward slashes with backward slashes
    {
        wchar_t * proc = norm_path;
        while (*proc) {
            if (*proc == L'/') {
                *proc = L'\\';
            }

            proc++;
        }
    }

    // remove duplicate backslashes (except the starting one)
    {
        wchar_t * proc = norm_path + 2;
        while (*proc) {
            if (*proc == L'\\' && *(proc + 1) == L'\\') {
                wcscpy(proc, proc + 1);
                continue;
            }

            proc++;
        }
    }

    // resolve '.' and '..'
    {
        wchar_t * dest = norm_path;
        wchar_t * path_segment = dest;

        wchar_t * proc = wcsstr(path_segment, L"\\.");
        while (proc) {
            if (proc[2] == '.' && (proc[3] == L'\\' || proc[3] == L'\0')) {
                // parent folder -> revert 1 path segment
                wcsncpy(dest, path_segment, proc - path_segment);
                dest += proc - path_segment;
                if (dest - 1 >= norm_path) {
                    while (*dest != L'\\')
                        dest--;
                }
                path_segment = proc + 3;
            } else if (proc[2] == L'\\' || proc[2] == L'\0') {
                // current folder -> ignore path segment
                wcsncpy(dest, path_segment, proc - path_segment);
                dest += proc - path_segment;
                path_segment = proc + 2;
            }

            proc = wcsstr(path_segment, L"\\.");
        }

        wcscpy(dest, path_segment);
    }

    return norm_path;
}


static McxStatus adapt_long_path(const wchar_t * path, wchar_t ** adapted_path) {
    *adapted_path = NULL;

    if (wcslen(path) < (MAX_PATH - 12)) {
        return RETURN_OK;
    }

    if (wcslen(path) >= 2 && path[0] == L'\\' && path[1] == L'\\') {
        // UNC path
        wchar_t * norm_path = normalize_unc_path(path);
        if (!norm_path) {
            mcx_log(LOG_ERROR, "Util: UNC path normalization failed");
            return RETURN_ERROR;
        }

        wchar_t * prefixed_norm_path = (wchar_t *) mcx_calloc(wcslen(LONG_UNC_PATH_PREFIX) + wcslen(norm_path) + 1, sizeof(wchar_t));
        if (!prefixed_norm_path) {
            mcx_log(LOG_ERROR, "Util: Extending UNC path failed");
            mcx_free(norm_path);
            return RETURN_ERROR;
        }

        wcscpy(prefixed_norm_path, LONG_UNC_PATH_PREFIX);
        wcscat(prefixed_norm_path, norm_path);

        mcx_free(norm_path);

        *adapted_path = prefixed_norm_path;
    } else {
        wchar_t * full_path = NULL;
        DWORD length = GetFullPathNameW(path, 0, NULL, NULL);
        if (length == 0) {
            mcx_log(LOG_ERROR, "Util: Retrieving absolute path length failed");
            print_last_error();
            return RETURN_ERROR;
        }

        full_path = (wchar_t *) mcx_malloc(sizeof(wchar_t) * length);
        if (!full_path) {
            mcx_log(LOG_ERROR, "Util: Absolute path memory allocation failed");
            return RETURN_ERROR;
        }

        length = GetFullPathNameW(path, length, full_path, NULL);
        if (length == 0) {
            mcx_log(LOG_ERROR, "Util: Absolute path creation failed");
            print_last_error();
            return RETURN_ERROR;
        }

        wchar_t * prefixed_norm_path = (wchar_t *) mcx_calloc(wcslen(LONG_PATH_PREFIX) + wcslen(full_path) + 1, sizeof(wchar_t));
        if (!prefixed_norm_path) {
            mcx_log(LOG_ERROR, "Util: Extending absolute path failed");
            mcx_free(full_path);
            return RETURN_ERROR;
        }

        wcscpy(prefixed_norm_path, LONG_PATH_PREFIX);
        wcscat(prefixed_norm_path, full_path);

        mcx_free(full_path);

        *adapted_path = prefixed_norm_path;
    }

    return RETURN_OK;
}


McxStatus mcx_dll_load(DllHandle * handle, const char * dllPath) {
    DllHandleCheck compValue;

    wchar_t * wDllPath = mcx_string_to_widechar(dllPath);
    wchar_t * wNormDllPath = NULL;

    McxStatus retVal = RETURN_OK;

    mcx_log(LOG_DEBUG, "Util: Loading dll: %ls", wDllPath);

    retVal = adapt_long_path(wDllPath, &wNormDllPath);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Util: Adapting dll path failed");
        goto cleanup;
    }

    if (wNormDllPath) {
        mcx_free(wDllPath);
        wDllPath = wNormDllPath;
    }

    mcx_log(LOG_DEBUG, "Util: Adapted dll path: %ls", wDllPath);

    * handle = LoadLibraryExW(wDllPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

    compValue = (DllHandleCheck) * handle;
    if (compValue <= HINSTANCE_ERROR) {
        mcx_log(LOG_ERROR, "Util: Dll (%s) could not be loaded", dllPath);
        print_last_error();
        retVal = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (wDllPath) {
        mcx_free(wDllPath);
    }

    return retVal;
}


void * mcx_dll_get_function(DllHandle dllHandle, const char* functionName) {
    void * fp = NULL;

    fp = (void *) GetProcAddress(dllHandle, (char *) functionName);

    return fp;
}

void mcx_dll_free(DllHandle dllHandle)
{
    FreeLibrary(dllHandle);
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */