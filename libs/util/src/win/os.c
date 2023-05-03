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
#include <windows.h>    // for SearchPath, CreateProcess, FormatMessageW
#include <winsock2.h>   // for WSAGetLastError

#include "common/memory.h"
#include "common/logging.h"

#include "util/os.h"
#include "util/paths.h"
#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void mcx_os_get_args(char *** argList, int * nArgs) {
    int i = 0;
    wchar_t ** wargv = CommandLineToArgvW(GetCommandLineW(), nArgs);

    char ** argv = (char **) mcx_calloc(*nArgs, sizeof(char*));

    for (i = 0; i < *nArgs; i++) {
        argv[i] = mcx_string_to_utf8(wargv[i]);
    }
    LocalFree(wargv);
    *argList = argv;
}

char * mcx_os_get_env_var(const char * name) {
    DWORD len = 32767 + 1; /* max environment variable length */

    wchar_t * wValue = (wchar_t *) mcx_calloc(len, sizeof(wchar_t));
    wchar_t * wName;

    char * value;

    int ret;

    wName = mcx_string_to_widechar(name);
    ret = GetEnvironmentVariableW(wName, wValue, len);
    if (!ret) {
        mcx_free(wName);
        mcx_free(wValue);
        return NULL;
    }
    value = mcx_string_to_utf8(wValue);

    mcx_free(wName);
    mcx_free(wValue);

    mcx_log(LOG_DEBUG, "getenv(%s)=%s", name, value);

    return value;
}

McxStatus mcx_os_set_env_var(const char * name, const char * value) {
    wchar_t * wName = mcx_string_to_widechar(name);
    wchar_t * wValue = mcx_string_to_widechar(value);

    if (!SetEnvironmentVariableW(wName, wValue)) {
        mcx_log(LOG_ERROR, "Could not set environment variable %s to %s", name, value);
        return RETURN_ERROR;
    }

    mcx_free(wName);
    mcx_free(wValue);

    return RETURN_OK;
}


const char * mcx_os_get_errno_descr(int errnum) {
  return _sys_errlist[errno];
}

int mcx_os_path_exists(const char * path) {
    int ret;
    char * absoluteNormalizedPath = NULL;
    wchar_t * wPath = NULL;

    absoluteNormalizedPath = mcx_os_path_normalize(path);
    if (path && !absoluteNormalizedPath) {
        mcx_log(LOG_ERROR, "Util: Could not convert path '%s' to a normalized absolute path", path);
        return 0;
    }

    wPath = mcx_string_to_widechar(absoluteNormalizedPath);

    if (absoluteNormalizedPath) {
        mcx_free(absoluteNormalizedPath);
    }

    ret = (_waccess(wPath, 0) != -1);
    if (wPath) {
        mcx_free(wPath);
    }

    return ret;
}

char * mcx_os_path_normalize(const char * path) {
    DWORD retVal = 0;
    DWORD len = 4096;
    wchar_t * wPath = NULL;
    wchar_t * wAbsPath = NULL;

    if (NULL == path) {
        return NULL;
    }

    wPath = mcx_string_to_widechar(path);
    if (!wPath) {
        return NULL;
    }

    wAbsPath = (wchar_t *) mcx_malloc(len * sizeof(wchar_t));
    if (!wAbsPath) {
        mcx_free(wPath);
        return NULL;
    }

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

McxStatus mcx_os_remove_dir_tree(const char * dir) {
    WIN32_FIND_DATAW ffd;
    wchar_t szDir[MAX_PATH];
    HANDLE hFile = INVALID_HANDLE_VALUE;
    wchar_t * wDir;

    McxStatus retVal = RETURN_OK;

    wDir = mcx_string_to_widechar(dir);

    wcsncpy(szDir, wDir, MAX_PATH);
    wcsncat(szDir, L"\\*", MAX_PATH);

    hFile = FindFirstFileW(szDir, &ffd);

    if (INVALID_HANDLE_VALUE == hFile) {
        char * dir = mcx_string_to_utf8(szDir);
        mcx_log(LOG_ERROR, "Util: No such file: %s", dir);
        mcx_free(dir);
        mcx_free(wDir);
        return RETURN_ERROR;
    }

    do {
        wcsncpy(szDir, wDir, MAX_PATH);
        wcsncat(szDir, L"\\", MAX_PATH);
        wcsncat(szDir, ffd.cFileName, MAX_PATH);

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(L".", ffd.cFileName) && wcscmp(L"..", ffd.cFileName)) {
                char * ddir = mcx_string_to_utf8(szDir);
                retVal = mcx_os_remove_dir_tree(ddir);
                mcx_free(ddir);
                if (RETURN_OK != retVal) {
                    mcx_free(wDir);
                    return RETURN_ERROR;
                }
            }
        } else {
            int ret;
            char * ddir = mcx_string_to_utf8(szDir);
            ret = _wremove(szDir);
            if (ret != 0) {
                mcx_log(LOG_DEBUG, "Util: 1 Deleting %s, %d, %s", ddir, ret, strerror(errno));
            } else {
                mcx_log(LOG_DEBUG, "Util: 1 Deleting %s", ddir);
            }
            mcx_free(ddir);
        }
    } while (FindNextFileW(hFile, &ffd) != 0);

    wcsncpy(szDir, wDir, MAX_PATH);
    {
        char * ddir = mcx_string_to_utf8(szDir);
        mcx_log(LOG_DEBUG, "Util: 2 Deleting %s", ddir);
        mcx_free(ddir);
    }
    _wrmdir(szDir);

    mcx_free(wDir);

    FindClose(hFile);

    return RETURN_OK;
}

char * mcx_os_get_current_dir(void) {
    char * cwd = NULL;
    size_t size = 0;
    wchar_t * buffer = NULL;

    /* MAX_PATH = 260 */
    buffer = (wchar_t *) mcx_malloc(MAX_PATH * sizeof(wchar_t));
    buffer = _wgetcwd(buffer, MAX_PATH);
    cwd = mcx_string_to_utf8(buffer);
    mcx_free(buffer);

    return cwd;
}

char * mcx_os_get_exe_dir(void) {
    DWORD wDirLen = 2048;
    size_t dirLen = 0;
    wchar_t * wDir = NULL;
    char * dir = NULL;

    size_t i = 0;

    wDir = (wchar_t *) mcx_malloc(wDirLen * sizeof(wchar_t));
    if (!wDir) {
        return NULL;
    }

    GetModuleFileNameW(NULL, wDir, wDirLen);

    dir = mcx_string_to_utf8(wDir);
    mcx_free(wDir);

    dirLen = strlen(dir);
    for (i = strlen(dir); dir[i] != PLATFORMPATHSIGN && i > 0; i--) ;

    dir[i] = '\0';
    dir = (char *) mcx_realloc(dir, strlen(dir) + 1);

    return dir;
}

int mcx_os_sleep_ms(unsigned int ms) {
    SetLastError(0);
    Sleep(ms);
    return GetLastError() ?-1 :0;
}

size_t mcx_os_process_create(char * args[]) {
    LPSTR filePart;
    char filename[MAX_PATH];

    char * cmd_str = NULL;

    size_t pid = -1;

    if (!SearchPath(NULL, args[0], ".exe", MAX_PATH, filename, &filePart)) {
        mcx_log(LOG_ERROR, "     %s not found in PATH, make sure the environment is set up correctly", args[0]);
        pid = -1;
        goto cleanup;
    }

    args[0] = filename;

    cmd_str = mcx_string_merge_quoted_array_with_spaces(args);
    if (!cmd_str) {
        pid = -1;
        goto cleanup;
    }

    mcx_log(LOG_DEBUG, "process_create: %s", cmd_str);

    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);
    if (!CreateProcess(NULL // lpApplicationName == NULL means the module name is the first white-space
                            // delimited token in cmd_str
                       , cmd_str // command line string including module name
                       , NULL
                       , NULL
                       , FALSE // do not inherit handles
                       , 0
                       , NULL // use environment of parent
                       , NULL // use working directory of parent
                       , &si
                       , &pi))
    {
        mcx_log(LOG_ERROR, "     Creating process for command %s failed", cmd_str);
        pid = -1;
        goto cleanup;
    } else {
        mcx_log(LOG_DEBUG, "     Creating process for command %s succeeded", cmd_str);
        pid = pi.dwProcessId;
    }

cleanup:
    if (cmd_str) {
        mcx_free(cmd_str);
    }

    // Handles in PROCESS_INFORMATION must be closed with CloseHandle when they are no longer needed.
    if (pid != -1) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return pid;
}

int mcx_os_mkdir(const char * dir) {
    int retVal;
    wchar_t * wDir = mcx_string_to_widechar(dir);

    retVal = _wmkdir(wDir);
    mcx_free(wDir);

    return retVal;
}

FILE * mcx_os_fopen(const char * path, const char * mode) {
    wchar_t * wPath = mcx_string_to_widechar(path);
    wchar_t * wMode = mcx_string_to_widechar(mode);

    // Open the file shareable (as opposed to _wfopen_s)
    FILE * f = _wfopen(wPath, wMode);

    mcx_free(wPath);
    mcx_free(wMode);
    return f;
}

char * mcx_os_win_get_last_error() {
    wchar_t * wError = NULL;
    char * utf8Error = NULL;

    FormatMessageW( FORMAT_MESSAGE_ALLOCATE_BUFFER
                    | FORMAT_MESSAGE_FROM_SYSTEM
                    | FORMAT_MESSAGE_IGNORE_INSERTS
                    , NULL
                    , WSAGetLastError()
                    , MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
                    , (LPWSTR)&wError
                    , 0
                    , NULL
        );

    utf8Error = mcx_string_to_utf8(wError);

    LocalFree(wError);

    return utf8Error;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */