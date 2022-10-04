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
#include <shlwapi.h>

#include "common/logging.h"
#include "common/memory.h"

#include "util/libs.h"
#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static void print_last_error() {
    LPVOID lpMsgBuf;
    DWORD err = GetLastError();
    switch (err) {
        case ERROR_BAD_EXE_FORMAT:
            mcx_log(LOG_ERROR, "Util: There is a mismatch in bitness (32/64) between current Model.CONNECT Execution Engine and the dynamic library");
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


McxStatus mcx_dll_load(DllHandle * handle, const char * dllPath) {
    DllHandleCheck compValue;

    wchar_t * wDllPath = mcx_string_to_widechar(dllPath);

    if (PathIsRelativeW(wDllPath)) {
        wchar_t * wFullDllPath = NULL;
        DWORD length = GetFullPathNameW(wDllPath, 0, NULL, NULL);
        if (length == 0) {
            mcx_log(LOG_ERROR, "Util: Error retrieving length of absolute path of Dll (%s)", wDllPath);
            print_last_error();
            return RETURN_ERROR;
        }
        wFullDllPath = (wchar_t *) mcx_malloc(sizeof(wchar_t) * length);
        length = GetFullPathNameW(wDllPath, length, wFullDllPath, NULL);
        if (length == 0) {
            mcx_log(LOG_ERROR, "Util: Error creating full path for Dll (%s)", wDllPath);
            print_last_error();
            return RETURN_ERROR;
        }
        mcx_free(wDllPath);
        wDllPath = wFullDllPath;
    }

    * handle = LoadLibraryExW(wDllPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    mcx_free(wDllPath);

    compValue = (DllHandleCheck) * handle;
    if (compValue <= HINSTANCE_ERROR) {
        mcx_log(LOG_ERROR, "Util: Dll (%s) could not be loaded", dllPath);
        print_last_error();
        return RETURN_ERROR;
    }

    return RETURN_OK;
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