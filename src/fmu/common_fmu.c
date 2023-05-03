/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "fmu/common_fmu.h"
#include "CentralParts.h"

#include "util/md5_file.h"
#include "util/paths.h"
#include "util/string.h"
#include "util/os.h"

#include "reader/model/components/specific_data/FmuInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void * JMCallbackMalloc(size_t size) {
    return mcx_calloc(1, size);
}

void * JMCallbackCalloc(size_t num, size_t size) {
    return mcx_calloc(num, size);
}

void * JMCallbackRealloc(void * ptr, size_t size) {
    return mcx_realloc(ptr, size);
}

void JMCallbackFree(void * ptr) {
    mcx_free(ptr);
}

static const char * FmuCommonGetName(const FmuCommon * fmu);

void JMCallbackLogger(jm_callbacks* c, jm_string module, jm_log_level_enu_t log_level, jm_string message) {

    const char * name = "Unknown FMU";

    if (NULL != c->context) {
        FmuCommon * fmu = (FmuCommon *) c->context;
        name = FmuCommonGetName(fmu);
        if (0 == strcmp(name, "")) {
            name = "Unknown FMU";
        }
    }

    switch (log_level) {
    case jm_log_level_fatal:
    case jm_log_level_error:
        mcx_log(LOG_ERROR, "%s: %s", name, message);
        break;

    case jm_log_level_warning:
        mcx_log(LOG_WARNING, "%s: %s", name, message);
        break;

    case jm_log_level_info:
        mcx_log(LOG_INFO, "%s: %s", name, message);
        break;

    case jm_log_level_verbose:
    case jm_log_level_debug:
        mcx_log(LOG_DEBUG, "%s: %s: %s", name, module, message);
        break;
    }
}

static jm_callbacks jmCallbacks = {JMCallbackMalloc,
                                   JMCallbackCalloc,
                                   JMCallbackRealloc,
                                   JMCallbackFree,
                                   JMCallbackLogger,
                                   jm_log_level_all,
                                   NULL /* context */,
                                   '\0' /* error message buffer */};

jm_callbacks * JMGetCallbacks(void) {
    return &jmCallbacks;
}


static char * GetExtractPath(char * tempDir, const char * name, char * pathToFmu) {
    char * extractPath = NULL;
    char * md5String = mcx_md5_file_fingerprint(pathToFmu);
    char * subDir = NULL;

    const char * pathList[2] = {tempDir, subDir};

    if (NULL == md5String) {
        const char defaultMD5[] = "0000";
        md5String = (char *) mcx_calloc(strlen(defaultMD5) + 1, sizeof(char));
        strcpy(md5String, defaultMD5);
    }

    subDir = (char *) mcx_calloc(strlen(name)+strlen(md5String)+2, sizeof(char));

    sprintf(subDir, "%s_%s", name, md5String);

    pathList[1] = subDir;

    mcx_path_merge(pathList, 2, &extractPath);

    if (NULL != md5String) {
        mcx_free(md5String);
    }
    if (NULL != subDir) {
        mcx_free(subDir);
    }

    return extractPath;
}


McxStatus CreateFmuExtractPath(FmuCommon * fmu, const char * name, const Config * config) {
    char * tempDir = NULL;
    char * encodedName = NULL;
    char * localName = NULL;

    if (NULL == config) {
        mcx_log(LOG_ERROR, "FMU: The config is not set correctly");
        return RETURN_ERROR;
    }

    tempDir = ConfigGetTempDir(config);

    if (NULL == tempDir) {
        tempDir = ConfigGetLocalTempDir(config);
        fmu->isLocal = TRUE;
    }

    // encode " " and "_" with escape char "_" for (OM)FMUs who cannot decode "%20"
    encodedName = mcx_string_encode_filename(name);
    if (encodedName) {
        localName = mcx_string_encode(encodedName, '_', " _");
        mcx_free(encodedName);
    }
    if (localName) {
        fmu->path = GetExtractPath(tempDir, localName, fmu->fmuFile);
        mcx_free(localName);
    } else {
        mcx_log(LOG_ERROR, "%s: could not encode name", name);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static const char * FmuCommonGetName(const FmuCommon * fmu) {
    // this function is often called for error or debug messages by the logger callback function
    // If an error occurs while the name is not yet available, an empty string will be returned
    if (NULL != fmu) {
        if (NULL != fmu->instanceName) {
            return fmu->instanceName;
        }
    }

    return "";
}

void FmuCommonInit(FmuCommon * fmu) {
    fmu->version = fmi_version_unknown_enu;
    fmu->context = NULL;
    fmu->fmuFile = NULL;
    fmu->path = NULL;
    fmu->instanceName = NULL;
    fmu->isLocal = 0;
}

McxStatus FmuCommonRead(FmuCommon * common, FmuInput * input) {
    McxStatus retVal = RETURN_OK;

        common->fmuFile = mcx_string_copy(input->fmuFile);
    return RETURN_OK;
}


void FmuCommonDestructor(FmuCommon * fmu) {
    if (fmu->context) {
        fmi_import_free_context(fmu->context);
    }
    if (fmu->path) {
        if (fmu->isLocal
            ) {
            McxStatus retVal = mcx_os_remove_dir_tree(fmu->path);
        }
        mcx_free(fmu->path);
    }

    if (fmu->instanceName) {
        mcx_free(fmu->instanceName);
    }
    if (fmu->fmuFile) {
        mcx_free(fmu->fmuFile);
    }
}

McxStatus FmuCommonSetup(FmuCommon * fmu) {
    jm_callbacks * callbacks = JMGetCallbacks();

    fmu->callbacks.calloc = callbacks->calloc;
    fmu->callbacks.malloc = callbacks->malloc;
    fmu->callbacks.realloc = callbacks->realloc;
    fmu->callbacks.free = callbacks->free;

    fmu->callbacks.context = (jm_voidp) fmu;

    fmu->callbacks.logger = callbacks->logger;

    fmu->callbacks.log_level = callbacks->log_level;

#ifdef MCX_DEBUG
    fmu->callbacks.log_level = jm_log_level_all;
#else
    fmu->callbacks.log_level = jm_log_level_fatal;
#endif //MCX_DEBUG

    // temporarily write all fmilib log levels
    // this should be changed within the fmilibrary
    // the fmu log levels should be independent of the fmilib log level
    fmu->callbacks.log_level = jm_log_level_all;

    if (fmu->fmuFile) {
        /* check if fmu exists */
        if (!mcx_os_path_exists(fmu->fmuFile)) {
            mcx_log(LOG_ERROR, "%s: %s does not exists", fmu->instanceName, fmu->fmuFile);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static void FmuCheckPathLength(FmuCommon * fmu) {
#ifdef OS_WINDOWS
    size_t len = strlen("/binaries/win32/") + strlen(".dll");
    if (fmu->path) {
        len += strlen(fmu->path);
    }
    if (fmu->instanceName) {
        len += strlen(fmu->instanceName);
    }
    if (len > 240) {
        mcx_log(LOG_WARNING, "FMU: The path to the binaries of the extracted FMU is longer than 240 characters.");
        mcx_log(LOG_WARNING, "     In case of problems, set the USE_TEMP_DIR environment variable to a shorter path.");
    }
#endif
}

McxStatus FmuOpen(FmuCommon * fmu, const struct Config * const config) {
    McxStatus retVal = RETURN_OK;

    fmu->context = fmi_import_allocate_context(&fmu->callbacks);

    {
        retVal = mcx_os_mkdir_recursive(fmu->path);

        if (RETURN_OK == retVal) {
            // fmilibrary does not normalize paths when joining them so they might get too long.
            // With mcx_path_get_absolute we normalize the path before.
            char * fmuFile = mcx_path_get_absolute(fmu->fmuFile);
            if (fmuFile) {
                mcx_free(fmu->fmuFile);
                fmu->fmuFile = fmuFile;
            }
            fmuFile = mcx_os_path_normalize(fmu->fmuFile);
            if (fmuFile) {
                mcx_free(fmu->fmuFile);
                fmu->fmuFile = fmuFile;
            }
            mcx_log(LOG_DEBUG, "%s: Unpacking %s to %s", fmu->instanceName, fmu->fmuFile, fmu->path);
            fmu->version = fmi_import_get_fmi_version(fmu->context, fmu->fmuFile, fmu->path);
        }
        else {
            mcx_log(LOG_ERROR, "%s: Cannot create %s to unpack %s", fmu->instanceName, fmu->path, fmu->fmuFile);
            retVal = RETURN_ERROR;
        }

    }

    return retVal;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */