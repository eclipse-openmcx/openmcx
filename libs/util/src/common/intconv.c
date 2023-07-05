/********************************************************************************
 * Copyright (c) 2023 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "util/intconv.h"

#include "common/status.h"
#include "common/logging.h"


#ifdef ENABLE_BOUND_CHECKS
#define MCX_INTEGER_CAST(src, trg, src_type, trg_type)\
    *trg = (trg_type) src;\
    if (sizeof(trg_type) != sizeof(src_type)) {\
        if ((src_type) *trg != src) {\
            mcx_log(\
                LOG_ERROR,\
                "Integer overflow: Converting %zu bit integer to %zu bit integer failed.",\
                sizeof(src_type) * 8,\
                sizeof(trg_type) * 8\
            );\
            return RETURN_ERROR;\
        }\
    }\
    return RETURN_OK;
#else
#define MCX_INTEGER_CAST(src, trg, src_type, trg_type)\
    *trg = (trg_type) src;\
    return RETURN_OK;
#endif


McxStatus mcx_int64_to_int(int64_t in, int * out) {
    MCX_INTEGER_CAST(in, out, int64_t, int)
}

McxStatus mcx_int64_to_char(int64_t in, char * out) {
    MCX_INTEGER_CAST(in, out, int64_t, char)
}

McxStatus mcx_int64_to_int_array(const int64_t * in, int * out, size_t num) {
    McxStatus ret = RETURN_OK;
    for (size_t j = 0; j < num; j++) {
        ret = mcx_int64_to_int(in[j], out + j);
#ifdef ENABLE_BOUND_CHECKS
        if (ret == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "Converting integer array failed");
            return RETURN_ERROR;
        }
#endif // ENABLE_BOUND_CHECKS
    }
    return RETURN_OK;
}

McxStatus mcx_int_to_int64_array(const int * in, int64_t * out, size_t num) {
    for (size_t j = 0; j < num; j++) {
        out[j] = (int64_t) in[j];
    }
    return RETURN_OK;
}