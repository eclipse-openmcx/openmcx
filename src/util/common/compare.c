/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "util/compare.h"

#include "CentralParts.h" // for ZERO_COMPARE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static double _double_cmp_eps = ZERO_COMPARE;

void double_set_eps(double eps) {
    _double_cmp_eps = eps;
}

double double_get_eps() {
    return _double_cmp_eps;
}

cmp double_cmp(double a, double b) {
    return double_cmp_eps(a, b, _double_cmp_eps);
}

/* Note: there is no one-size-fits-all variant when it comes to comparing
 * floating point numbers.
 *
 * See https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 * for an introduction.
 */
int double_almost_equal(double a, double b, double eps) {
    double largest;
    double diff = (a - b);
    diff = (diff < 0) ? -diff : diff;    // do not use fabs for performance reasons

    // compare numbers close to 0.0 with absolute epsilon check
    if (diff < eps) {
        return 1;
    }

    // do not use fabs for performance reasons
    a = (a < 0) ? -a : a;
    b = (b < 0) ? -b : b;

    largest = (a > b) ? a : b;

    // compare large numbers with relative epsilon check
    if (diff < largest * eps) {
        return 1;
    } else {
        return 0;
    }
}

static int double_almost_equal_abs(double a, double b, double eps) {
    double diff = (a - b);
    diff = (diff < 0) ? -diff : diff;
    return diff < eps;
}

cmp double_cmp_eps(double a, double b, double eps) {
    if (double_almost_equal(a, b, eps)) {
        return CMP_EQ;
    } else if (a < b) {
        return CMP_LT;
    } else if (a > b) {
        return CMP_GT;
    } else {
        // error
        return CMP_IN;
    }
}

cmp double_cmp_eps_abs(double a, double b, double eps) {
    if (double_almost_equal_abs(a, b, eps)) {
        return CMP_EQ;
    } else if (a < b) {
        return CMP_LT;
    } else if (a > b) {
        return CMP_GT;
    } else {
        // error
        return CMP_IN;
    }
}

int double_eq(double a, double b) {
    return (double_cmp(a,b) == CMP_EQ);
}

int double_lt(double a, double b) {
    return (double_cmp(a,b) == CMP_LT);
}

int double_gt(double a, double b) {
    return (double_cmp(a,b) == CMP_GT);
}

int double_leq(double a, double b) {
    return (double_cmp(a,b) == CMP_EQ
            || double_cmp(a,b) == CMP_LT);
}

int double_geq(double a, double b) {
    return (double_cmp(a,b) == CMP_EQ
            || double_cmp(a,b) == CMP_GT);
}

int double_leq_eps_abs(double a, double b, double eps) {
    return (double_cmp_eps_abs(a, b, eps) == CMP_EQ || double_cmp_eps_abs(a, b, eps) == CMP_LT);
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */