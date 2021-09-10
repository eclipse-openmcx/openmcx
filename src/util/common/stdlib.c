/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"

#include "util/stdlib.h"

#include <stdlib.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void * mcx_copy(void * object, size_t size) {
    void * copy = mcx_malloc(size);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, object, size);

    return copy;
}

size_t mcx_filter(void * dst, void * base, size_t nmemb, size_t size, int (*pred)(const void *, void *), void * arg) {
    size_t i;
    size_t j = 0;

    for (i = 0; i < nmemb; i++) {
        if (pred((char *)base + i*size, arg)) {
            memcpy((char *)dst + j*size, (char *)base + i*size, size);

            j += 1;
        }
    }

    return j;
}

size_t mcx_nub_by(void * base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void (*destr)(void *, void *), void * arg) {
    size_t i;
    size_t j;

    size_t nubbed = 0;

    for (i = 1; i < nmemb; i++) {
        if (!compar((char *)base + (i-1)*size, (char *)base + i*size, arg)) {
            if (destr) {
                destr((char *)base + i*size, arg);
            }

            for (j = i; j < nmemb - 1; j++) {
                memcpy((char *)base + j*size, (char *)base + (j+1)*size, size);
            }

            // Clear last element
            memset((char *)base + (nmemb-1)*size, 0, size);
            nmemb -= 1;

            nubbed += 1;

            // Reset i to compare again with current element
            i -= 1;
        }
    }

    return nubbed;
}

// calculates the number of digits of an unsigned integer
// this rather unusual implementation is inspired by a talk given by A. Alexandrescu
// (https://www.youtube.com/watch?v=vrfYLlR8X8k at about 1:06:00)
// it seems to be quite fast with current CPUs
uint32_t mcx_digits10(uint64_t v) {
    uint32_t result = 1;
    for (;;) {
        if (v < 10) return result;
        if (v < 100) return result + 1;
        if (v < 1000) return result + 2;
        if (v < 10000) return result + 3;
        v /= 10000U;
        result += 4;
    }
}

/**
 * Released under the MIT License - https://opensource.org/licenses/MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Based on http://www.davekoelle.com/files/alphanum.hpp */
int alphanum_isdigit(const char c)
{
    return c >= '0' && c <= '9';
}
int mcx_natural_sort_cmp(const char * l, const char * r) {
    enum mode_t { STRING, NUMBER } mode=STRING;

    while(*l && *r)
    {
        if(mode == STRING)
        {
            char l_char, r_char;
            while((l_char=*l) && (r_char=*r))
            {
                // check if this are digit characters
                const int l_digit= alphanum_isdigit(l_char), r_digit= alphanum_isdigit(r_char);
                // if both characters are digits, we continue in NUMBER mode
                if(l_digit && r_digit)
                {
                    mode=NUMBER;
                    break;
                }
                // if only the left character is a digit, we have a result
                if(l_digit) return -1;
                // if only the right character is a digit, we have a result
                if(r_digit) return +1;
                // compute the difference of both characters
                const int diff=l_char - r_char;
                // if they differ we have a result
                if(diff != 0) return diff;
                // otherwise process the next characters
                ++l;
                ++r;
            }
        }
        else // mode==NUMBER
        {
            // get the left number
            unsigned long l_int = 0;
            while (*l && alphanum_isdigit(*l))
            {
                if (l_int < ULONG_MAX / 10 - 10) {
                    l_int = l_int * 10 + *l - '0';
                } else {
                    l_int = ULONG_MAX;
                }
                ++l;
            }

            // get the right number
            unsigned long r_int = 0;
            while (*r && alphanum_isdigit(*r))
            {
                if (r_int < ULONG_MAX / 10 - 10) {
                    r_int = r_int * 10 + *r - '0';
                } else {
                    r_int = ULONG_MAX;
                }
                ++r;
            }

            // if the difference is not equal to zero, we have a comparison result
            const long diff=l_int-r_int;
            if(diff != 0)
                return diff;

            // otherwise we process the next substring in STRING mode
            mode=STRING;
        }
    }

    if(*r) return -1;
    if(*l) return +1;
    return 0;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */