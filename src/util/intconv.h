/********************************************************************************
 * Copyright (c) 2023 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_INTCONV_H
#define MCX_UTIL_INTCONV_H


#include <stdint.h>

#include "common/status.h"


McxStatus mcx_int64_to_int(int64_t in, int * out);
McxStatus mcx_int64_to_char(int64_t in, char * out);
/** `out` param must already have space allocated for `num` int's */
McxStatus mcx_int64_to_int_array(const int64_t * in, int * out, size_t num);
/** `out` param must already have space allocated for `num` int64_t's */
McxStatus mcx_int_to_int64_array(const int * in, int64_t * out, size_t num);


#endif // MCX_UTIL_INTCONV_H