/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <assert.h>
#include <stddef.h>
#include <strings.h>

#include "common/logging.h"

#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

wchar_t * mcx_string_to_widechar(const char * str) {
    mcx_log(LOG_ERROR, "mcx_string_to_widechar: Not implemented yet");
    assert(0 && "mcx_string_to_widechar is not implemented yet");
    return NULL;
}

char * mcx_string_to_utf8(const wchar_t * wstr) {
    mcx_log(LOG_ERROR, "mcx_string_to_utf8: Not implemented yet");
    assert(0 && "mcx_string_to_utf8 is not implemented yet");
    return NULL;
}

char * mcx_string_mbcs_to_utf8(const char * str) {
    mcx_log(LOG_DEBUG, "Converting string %s. Assumption: it is already UTF-8 encoded.", str);
    return mcx_string_copy(str);
}

int mcx_string_cmpi(const char * left, const char * right) {
    return strcasecmp(left, right);
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */