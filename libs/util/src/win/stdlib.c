/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "util/stdlib.h"

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct {
    int (*compar)(const void *, const void *, void *);
    void * arg;
} qsort_s_data;

static int qsort_s_swap_compar(void * ctx, const void * a, const void * b) {
    qsort_s_data * data = (qsort_s_data *) ctx;
    return data->compar(a, b, data->arg);
}

void mcx_sort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg) {
    qsort_s_data data;
    data.compar = compar;
    data.arg = arg;

    qsort_s(base, nmemb, size, qsort_s_swap_compar, &data);
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */