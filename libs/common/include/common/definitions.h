/********************************************************************************
 * Copyright (c) 2023 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_COMMON_DEFINITIONS_H
#define MCX_COMMON_DEFINITIONS_H

#ifdef OS_LINUX
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE
#endif // OS_LINUX

#define NUM_PI 3.141592653589793

#define TRUE   1
#define FALSE  0

#define UNUSED(x) (void)(x)

#include <math.h>

#if defined _MSC_VER
#ifndef __cplusplus
#ifndef fmax
#define fmax( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif /* fmax */
#ifndef fmin
#define fmin( a, b ) ( ((a) < (b)) ? (a) : (b) )
#endif /* fmin */
#endif /* __cplusplus */
#endif /* _MSC_VER */


#endif // MCX_COMMON_DEFINITIONS_H