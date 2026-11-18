/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <dlfcn.h>
#include <stddef.h>

#include "common/logging.h"

#include "util/libs.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static McxStatus __mcx_dll_load(DllHandle * handle, const char * dllPath, int flags) {
    *handle = dlopen(dllPath, flags);
    if (!*handle) {
        mcx_log(LOG_ERROR, "Could not load shared object %s:", dllPath);
        mcx_log(LOG_ERROR, "%s", dlerror());
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus mcx_dll_load(DllHandle * handle, const char * dllPath) {
#if(__APPLE__)
    return __mcx_dll_load(handle, dllPath, RTLD_LAZY|RTLD_LOCAL);
#else
    return __mcx_dll_load(handle, dllPath, RTLD_LAZY|RTLD_LOCAL|RTLD_DEEPBIND);
#endif
}

McxStatus mcx_dll_load_global(DllHandle * handle, const char * dllPath) {
#if(__APPLE__)
    return __mcx_dll_load(handle, dllPath, RTLD_LAZY|RTLD_GLOBAL);
#else
    return __mcx_dll_load(handle, dllPath, RTLD_LAZY|RTLD_GLOBAL|RTLD_DEEPBIND);
#endif
}

void * mcx_dll_get_function(DllHandle dllHandle, const char* functionName) {
    void * fp = NULL;

    fp = dlsym(dllHandle, functionName);

    return fp;
}

void mcx_dll_free(DllHandle dllHandle)
{
    dlclose(dllHandle);
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */
