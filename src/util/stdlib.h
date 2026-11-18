/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_STDLIB_H
#define MCX_UTIL_STDLIB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * Sorts the given array in-place using quicksort.
 *
 * Note: This function is platform-independent and expects arguments
 * just like for qsort_r, but on all platforms.
 *
 * @param base basepointer to array to be sorted
 * @param nmemb number of elements in the base array
 * @param size size of each array element
 * @param compar function comparing elements of the array (-1 <, 0 ==,1 >).
 * The first two arguments to compar are the elements, while the third
 * is the context passed to mcx_sort.
 * @param arg context for the compar function
 */

#if(__APPLE__)
void mcx_sort(void *base, size_t nmemb, size_t size,
              int (*compar)(void *, const void *, const void *), void *arg);
#else
void mcx_sort(void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *, void *), void *arg);
#endif
int mcx_natural_sort_cmp(const char * left, const char * right);

/**
 * Takes all elements that are satisfying the given predicate.
 *
 * @param dst array that matching elements are copied into. It needs
 * to be at least as big as number of matching elements times size per element
 * @param base array that the elements are taken from
 * @param nmemb number of elements in array
 * @param size size of each element in array
 * @param pred function returning whether the given element should be
 * taken or not
 * @param arg context for the predicate function
 * @return number of elements that satisfied the predicate
 */
size_t mcx_filter(void * dst, void * base, size_t nmemb, size_t size, int (*pred)(const void *, void *), void * arg);

/**
 * Removes consecutive equal elements from an array, leaving only
 * unique elements. (Taking the essence of the array, i.e. the "nub")
 *
 * @param base array that the elements are taken from
 * @param nmemb number of elements in array
 * @param size size of each element in array
 * @param compar function returning whether two given elements are equal
 * @param destr destructor for elements in the array. Can be NULL.
 * @param arg context for the compare function
 * @return number of elements that have been removed
 */
size_t mcx_nub_by(void * base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void (*destr)(void *, void *), void * arg);

uint32_t mcx_digits10(uint64_t v);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_UTIL_STDLIB_H