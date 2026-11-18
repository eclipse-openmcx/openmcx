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
#include <ftw.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for usleep, fork, execvp

#include "common/logging.h"
#include "common/memory.h"

#include "util/os.h"
#include "util/paths.h"
#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void mcx_os_get_args(char *** argList, int * nArgs) {
    *argList = NULL;
    *nArgs = 0;
}

char * mcx_os_get_env_var(const char * name) {
    char * value = getenv(name);
    mcx_log(LOG_DEBUG, "getenv(%s)=%s", name, value);
    if (value) {
        char * buffer = (char *) mcx_calloc(strlen(value) + 1, sizeof(char));
        strcpy(buffer, value);
        return buffer;
    } else {
        return NULL;
    }
}

McxStatus mcx_os_set_env_var(const char * name, const char * value) {
    int overwrite = 1;

    if (setenv(name, value, overwrite)) {
        mcx_log(LOG_ERROR, "Could not set environment variable %s to %s", name, value);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}


const char * mcx_os_get_errno_descr(int errnum) {
  return strerror(errno);
}

int mcx_os_path_exists(const char * path) {
    return (access(path, 0) != -1);
}

char * mcx_os_path_normalize(const char * path) {
    char * tmp = NULL;
    char * normalizedPath = NULL;

    tmp = realpath(path, NULL);

    if (tmp) {
        normalizedPath = mcx_string_copy(tmp);
        free(tmp); // result of realpath needs to be free'd with free
    }

    return normalizedPath;
}

static int
_remove_file(const char *fpath, const struct stat *sb,
            int tflag, struct FTW *ftwbuf) {
    int retVal = remove(fpath);

    if (retVal) {
        mcx_log(LOG_ERROR, "Util: Could not delete file %s", fpath);
    }

    return retVal;
}

McxStatus mcx_os_remove_dir_tree(const char * dir) {
    int retVal = nftw(dir, _remove_file, 64 /* depth */, FTW_DEPTH | FTW_PHYS);
    if (retVal) { return RETURN_ERROR; }
    else { return RETURN_OK; }
}

char * mcx_os_get_current_dir(void) {
    char * cwd = NULL;
    size_t size = 0;

    size = pathconf(".", _PC_PATH_MAX);
    if ((cwd = (char *) mcx_malloc(size+1)) != NULL) {
        cwd = getcwd(cwd, size+1);
    }

    return cwd;
}

char * mcx_os_get_exe_dir(void) {
    size_t dirSize = 2048;
    char * dir = NULL;

    char * procPath = NULL;
    ssize_t size = 0;

    size_t i = 0;

    dir = (char *) mcx_malloc(dirSize * sizeof(char));
    if (!dir) {
        return NULL;
    }

    procPath = (char *) mcx_malloc(dirSize * sizeof(char));
    if (!procPath) {
        return NULL;
    }

    sprintf(procPath, "/proc/%d/exe", getpid());
    size = readlink(procPath, dir, dirSize);
    if (size < 0) {
      mcx_free(procPath);
      mcx_free(dir);
      return NULL;
    }
    dir[size] = '\0';
    mcx_free(procPath);

    for (i = 0; i < dirSize && dir[i] != '\0'; i++) ;
    for (     ; dir[i] != PLATFORMPATHSIGN; i--) ;

    dir[i] = '\0';
    dir = (char *) mcx_realloc(dir, strlen(dir) + 1);

    return dir;
}

int mcx_os_mkdir(const char * dir) {
    int retVal;

    retVal = mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    return retVal;
}

int mcx_os_sleep_ms(unsigned int ms){
    return usleep(1000 * ms);
}

size_t mcx_os_process_create(char * args[]) {
    pid_t pid = fork();

    char * cmd_str = mcx_string_merge_array_with_spaces(args);
    if (!cmd_str) {
        pid = -1;
        goto cleanup;
    }

    mcx_log(LOG_DEBUG, "process_create: %s", cmd_str);

    if (!pid) {
        if (execvp(args[0], args) == -1) {
            mcx_log(LOG_ERROR, "Creating process for command %s failed", cmd_str);
            pid = -1;
        }
    } else if (pid == -1) {
        mcx_log(LOG_ERROR, "     Creating process for command %s failed", cmd_str);
        pid = -1;
    } else {
        mcx_log(LOG_DEBUG, "     Creating process for command %s succeeded, PID: %d", cmd_str, pid);
    }

cleanup:

    if (cmd_str) {
        mcx_free(cmd_str);
    }

    return (size_t) pid;
}

FILE * mcx_os_fopen(const char * path, const char * mode) {
    return fopen(path, mode);
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */