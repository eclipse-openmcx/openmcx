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
#include <stdlib.h>

#include "common/memory.h"
#include "common/logging.h"

#include "util/paths.h"


#define LOCATION_PREFIX "file://"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char * mcx_path_dir_name(const char * fileName) {
    char * dir = NULL;
    int n = 0;

    n = (int) strlen(fileName);
    dir = (char *) mcx_malloc((n + 1) * sizeof(char));
    if (!dir) { return NULL; }

    strncpy(dir, fileName, (n + 1));

    while (n >= 0 && dir[n] != PLATFORMPATHSIGN) {
        dir[n] = '\0';
        n--;
    }
    if (dir[0] == '\0') {
        dir[0] = '.';
    }
    return dir;
}

char * mcx_path_file_name(const char * path) {
    char * dir = NULL;
    int strlengthPathName = 0, strlengthPath = 0;

    strlengthPathName = strlengthPath = (int) strlen(path);

    while (strlengthPathName >= 0 && path[strlengthPathName] != PLATFORMPATHSIGN) {
        strlengthPathName--;
    }
    strlengthPathName++;

    dir = (char *)mcx_malloc((strlengthPath - strlengthPathName + 1) * sizeof(char));
    if (!dir) { return NULL; }

    strncpy(dir, &path[strlengthPathName], (strlengthPath - strlengthPathName + 1));

    return dir;
}

void mcx_path_strip_ext(char * path) {
    char * dot = strrchr(path, '.');

    if (dot) {
        *dot = '\0';
    }
}

char ** mcx_path_split(char * path) {
    size_t partsNum = 0;
    size_t pos = 0;
    size_t i = 0;
    size_t len = strlen(path);

    /* pointer to first chars of parts in path */
    char ** partsPtr = NULL;

    /* array of parts */
    char ** parts = NULL;

    if (path[pos] != PLATFORMPATHSIGN) {
        partsNum++;
    }
    for (pos = 0; pos < len; pos++) {
        if (path[pos] == PLATFORMPATHSIGN && path[pos + 1]) {
            partsNum++;
        }
    }

    partsPtr = (char **)mcx_calloc(sizeof(char*), (partsNum + 1));
    if (NULL == partsPtr) {
        return NULL;
    }

    // loop a second time to avoid realloc
    pos = 0;
    partsNum = 0;
    if (path[pos] != PLATFORMPATHSIGN) {
        partsPtr[partsNum] = &path[pos];
        partsNum++;
    }
    for (pos = 0; pos < len; pos++) {
        if (path[pos] == PLATFORMPATHSIGN && path[pos + 1]) {
            partsPtr[partsNum] = &path[pos + 1];
            partsNum++;
        }
    }

    if (path[pos] == PLATFORMPATHSIGN) {
        partsPtr[partsNum] = &path[pos];
    } else {
        partsPtr[partsNum] = &path[pos+1];
    }

    parts = (char **)mcx_calloc(partsNum + 1, sizeof(char *));
    if (NULL == parts) {
        return NULL;
    }

    for (i = 0; i < partsNum; i++) {
        size_t partLen = (partsPtr[i + 1] - partsPtr[i]) - 1; /* -1 for pathsign */
        char * buffer = (char *)mcx_calloc(partLen + 1, sizeof(char));
        if (NULL == buffer) {
            return NULL;
        }
        strncpy(buffer, partsPtr[i], partLen);
        buffer[partLen] = '\0';
        parts[i] = buffer;
    }
    mcx_free(partsPtr);

    parts[partsNum] = NULL;

    return parts;
}

static void mcx_free_string_array(char ** array) {
    size_t i = 0;

    if (array) {
        while (array[i]) {
            mcx_free(array[i]);
            i++;
        }
        mcx_free(array);
    }
}

char * mcx_path_get_relative(const char * fileName, const char * path) {
    char * absFile = NULL;
    char * absPath = NULL;

    char ** fileParts = NULL;
    char ** pathParts = NULL;

    char ** relPath = NULL;
    size_t relPathNum = 0;

    size_t prefixNum = 0;

    char * mergedPath = NULL;

    size_t i = 0;

    absFile = mcx_path_get_absolute(fileName);
    absPath = mcx_path_get_absolute(path);
    if (!absFile || !absPath) {
        goto cleanup;
    }

    fileParts = mcx_path_split(absFile);
    pathParts = mcx_path_split(absPath);

    if (!fileParts || !pathParts) {
        goto cleanup;
    }


#if defined (OS_WINDOWS)
    while (fileParts[prefixNum] &&
           pathParts[prefixNum] &&
           0 == _strcmpi(fileParts[prefixNum], pathParts[prefixNum])) {
        prefixNum += 1;
    }

    if (_strcmpi(fileParts[0], pathParts[0])) {
        /* the paths are on different drives, no relative path is possible */
        goto cleanup;
    }
#endif
    while (fileParts[prefixNum] &&
           pathParts[prefixNum] &&
           0 == strcmp(fileParts[prefixNum], pathParts[prefixNum])) {
        prefixNum += 1;
    }

    i = prefixNum;
    while (pathParts[i]) {
        relPathNum++;
        i++;
    }
    i = prefixNum;
    while (fileParts[i]) {
        relPathNum++;
        i++;
    }

    relPath = (char * *) mcx_calloc(relPathNum, sizeof(char*));
    if (NULL == relPath) {
        goto cleanup;
    }

    size_t relPathIndex = 0;
    i = prefixNum;
    while (pathParts[i]) {
        char * buffer = (char *) mcx_calloc(3, sizeof(char));
        if (NULL == buffer) {
            goto cleanup;
        }
        strncpy(buffer, "..", 2);
        relPath[relPathIndex] = buffer;
        i++;
        relPathIndex++;
    }

    i = prefixNum;
    while (fileParts[i]) {
        char * buffer = (char *) mcx_calloc(strlen(fileParts[i]) + 1, sizeof(char));
        if (NULL == buffer) {
            goto cleanup;
        }
        strncpy(buffer, fileParts[i], strlen(fileParts[i]));
        relPath[relPathIndex] = buffer;
        i++;
        relPathIndex++;
    }
    mcx_path_merge((const char * *) relPath, relPathNum, &mergedPath);

cleanup:
    if (absFile) { mcx_free(absFile); }
    if (absPath) { mcx_free(absPath); }

    mcx_free_string_array(pathParts);
    mcx_free_string_array(fileParts);

    if (relPath) {
        for (i = 0; i < relPathNum; i++) {
            if (relPath[i]) { mcx_free(relPath[i]); }
        }
        mcx_free(relPath);
    }

    return mergedPath;
}


char * mcx_path_to_uri(const char * path) {
    char * uri = NULL;
    char * absPath = mcx_path_get_absolute(path);
    if (!absPath) {
        mcx_log(LOG_ERROR, "Util: Could not get absolute path of %s", path);
        return NULL;
    }

    uri = (char *) mcx_calloc(strlen(absPath) + strlen(LOCATION_PREFIX) + 1, sizeof(char));
    if (!uri) { return NULL; }

    strcpy(uri, LOCATION_PREFIX);
    strcat(uri, absPath);

    mcx_free(absPath);

    return uri;
}

void mcx_path_convert_sep(char * path, char newSep) {
    size_t i;
    size_t len;

    if (!path) {
        return;
    }

    len = strlen(path);

    for (i = 0; i < len; i++) {
        if (UNIXPATHSIGN == path[i] || PCPATHSIGN == path[i]) {
            path[i] = newSep;
        }
    }
}

void mcx_path_to_platform(char * path) {
    mcx_path_convert_sep(path, PLATFORMPATHSIGN);
}

char * mcx_path_join(const char * path1, const char * path2) {
    const char * pathList[] = {path1, path2};

    char * joinedPath = NULL;

    int status = 0;

    status = mcx_path_merge(pathList, 2, &joinedPath);
    if (status) {
        return NULL;
    }

    return joinedPath;
}

int mcx_path_merge(const char * * pathList, size_t numbOfStrings, char * * returnMergedPath)
{
    size_t pathLength=0;
    size_t actStrLength = 0;
    size_t i;

    for (i = 0; i < numbOfStrings; i++) {
        if (pathList[i]) {
            pathLength += strlen(pathList[i]);
        }
    }

    *returnMergedPath = (char *) mcx_malloc((pathLength + numbOfStrings + 5) * sizeof(char));

    (*returnMergedPath)[0] = '\0';

    for (i = 0; i < numbOfStrings; i++) {
        if (pathList[i]) {
            strcat(*returnMergedPath, pathList[i]);
            actStrLength = strlen(*returnMergedPath);
            if (0 == actStrLength) {
                actStrLength = 1;
            }
            if (PLATFORMPATHSIGN != (*returnMergedPath)[actStrLength-1] && i < (numbOfStrings-1)) {
                if (*pathList[0] == '\0' && i == 0){
                    (*returnMergedPath)[actStrLength++] = '.';
                    (*returnMergedPath)[actStrLength] = PLATFORMPATHSIGN;
                } else {
                    (*returnMergedPath)[actStrLength] = PLATFORMPATHSIGN;
                }
                (*returnMergedPath)[actStrLength+1] = '\0';
            }
        }
    }

    (*returnMergedPath)[actStrLength] = '\0';
    mcx_path_to_platform(*returnMergedPath);

    return 0;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */