/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <string.h>
#include <unistd.h>

#include "common/memory.h"
#include "common/logging.h"

#include "util/os.h"
#include "util/paths.h"
#include "util/string.h"


#define LOCATION_PREFIX "file://"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char * mcx_path_get_absolute(const char * path) {
    char * curDir = NULL;
    const char * pathList[2];
    char * absPath = NULL;

    if (mcx_path_is_absolute(path)) {
        return mcx_string_copy(path);
    }
    curDir = mcx_os_get_current_dir();

    pathList[0] = curDir;
    pathList[1] = path;

    mcx_path_merge(pathList, 2, &absPath);
    mcx_free(curDir);

    return absPath;
}

int mcx_path_is_absolute(const char * path) {
    if (strlen(path) > 0) {
        return path[0] == '/';
    }

    return 0;
}

char * mcx_path_from_uri(const char * uri) {
    char * buffer = NULL;

    /* check if this is file uri */
    if (strncmp(uri, LOCATION_PREFIX, strlen(LOCATION_PREFIX))) {
        /* not a file uri */
        return NULL;
    }

    /* check if uri has authority component */
    if (uri[strlen(LOCATION_PREFIX)] == '/') {
        /* no authority component */
        uri += strlen(LOCATION_PREFIX);
    } else {
        /* skip over authority component */
        uri += strlen(LOCATION_PREFIX);
        while (*uri != '\0' && *uri != '/') {
            uri++;
        }
    }

    buffer = mcx_string_decode(uri, '%');
    if (buffer == NULL) {
        mcx_log(LOG_ERROR, "Cannot URL decode string");
        return NULL;
    }

    return buffer;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */