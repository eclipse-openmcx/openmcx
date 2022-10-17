/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_COMPARE_H
#define MCX_UTIL_COMPARE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef enum {
    CMP_EQ = 0,
    CMP_LT = -1,
    CMP_GT = 1,
    CMP_IN = 42
} cmp;


/**
 * Set the default precision used for double_cmp functions
 */
void double_set_eps(double eps);

/**
 * Get the default precision
 */
double double_get_eps();

/**
 * Returns CMP_LT, CMP_EQ or CMP_GT if a < b, a == b, a > b within an epsilon
 * and CMP_IN if a and b are not comparable
 */
cmp double_cmp(double a, double b);

/**
 * Returns CMP_LT, CMP_EQ or CMP_GT if a < b, a == b, a > b within eps
 * and CMP_IN if a and b are not comparable
 */
cmp double_cmp_eps(double a, double b, double eps);

/**
 * Returns CMP_LT, CMP_EQ or CMP_GT if a < b, a == b, a > b within eps
 * and CMP_IN if a and b are not comparable.
 *
 * The comparison is done in an absolute manner.
 */
cmp double_cmp_eps_abs(double a, double b, double eps);

/**
 * Returns if a == b within an epsilon
 */
int double_eq(double a, double b);

/**
 * Returns if a < b modulo an epsilon
 */
int double_lt(double a, double b);

/**
 * Returns if a > b modulo an epsilon
 */
int double_gt(double a, double b);

/**
 * Returns if a <= b modulo an epsilon
 */
int double_leq(double a, double b);

/**
 * Returns if a => b modulo an epsilon
 */
int double_geq(double a, double b);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_UTIL_COMPARE_H