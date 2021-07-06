/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CHANNEL_VALUE_H
#define MCX_CORE_CHANNELS_CHANNEL_VALUE_H

#include "CentralParts.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// possible types of values that can be put on channels
typedef enum ChannelType {
    CHANNEL_UNKNOWN = 0,
    CHANNEL_DOUBLE = 1,
    CHANNEL_INTEGER = 2,
    CHANNEL_BOOL = 3,
    CHANNEL_STRING = 4,
    CHANNEL_BINARY = 5,
    CHANNEL_BINARY_REFERENCE = 6,
    CHANNEL_ARRAY = 7,
} ChannelType;

typedef struct {
    double startTime;
    double endTime;
} TimeInterval;

typedef struct {
    double (* fn)(TimeInterval * arg, void * env);
    void * env;
} proc;

typedef struct {
    size_t len;
    char * data;
} binary_string;

typedef struct {
    size_t numDims;
    size_t * dims;
    ChannelType type;
    void * data;
} array;

int array_dims_match(array * a, array * b);
size_t array_num_elements(array * a);

// TODO: change concrete types to McxDouble, McxInteger, etc
typedef union ChannelValueData {
    /* the order is significant. double needs to be the first entry for union initialization to work */
    double d;
    int i;
    char * s;
    binary_string b;
    array a;
} ChannelValueData;

typedef struct ChannelValue {
    ChannelType type;
    ChannelValueData value;
} ChannelValue;

void   ChannelValueInit(ChannelValue * value, ChannelType type);
void ChannelValueDestructor(ChannelValue * value);
char * ChannelValueToString(ChannelValue * value);
McxStatus ChannelValueDataToStringBuffer(const ChannelValueData * value, ChannelType type, char * buffer, size_t len);
McxStatus ChannelValueToStringBuffer(const ChannelValue * value, char * buffer, size_t len);

ChannelType ChannelValueType(ChannelValue * value);
void *      ChannelValueReference(ChannelValue * value);

void ChannelValueDataDestructor(ChannelValueData * data, ChannelType type);
void ChannelValueDataInit(ChannelValueData * data, ChannelType type);
McxStatus ChannelValueDataSetFromReference(ChannelValueData * data, ChannelType type, const void * reference);

McxStatus ChannelValueSetFromReference(ChannelValue * value, const void * reference);
McxStatus ChannelValueSetToReference(ChannelValue * value, void * reference);

McxStatus ChannelValueSet(ChannelValue * value, const ChannelValue * source);

size_t ChannelValueTypeSize(ChannelType type);
int ChannelTypeMatch(ChannelType a, ChannelType b);


ChannelValue ** ArrayToChannelValueArray(void * values, size_t num, ChannelType type);

/*
 * Returns a string representation of ChannelType for use in log
 * messages.
 */
const char * ChannelTypeToString(ChannelType type);

/*
 * Creates a copy of value. Allocates memory if needed, e.g. when
 * value is of type CHANNEL_STRING.
 */
ChannelValue * ChannelValueClone(ChannelValue * value);

/*
 * If both val1 and val2 have the same ChannelType that is either
 * CHANNEL_DOUBLE or CHANNEL_INTEGER then ChannelValueLeq returns the
 * respective result for (<=).
 *
 * If the types do not match or if the types are not CHANNEL_DOUBLE or
 * CHANNEL_INTEGER, then it returns FALSE.
 */
int ChannelValueLeq(ChannelValue * val1, ChannelValue * val2);

/*
 * If both val1 and val2 have the same ChannelType that is either
 * CHANNEL_DOUBLE or CHANNEL_INTEGER then ChannelValueLeq returns the
 * respective result for (>=).
 *
 * If the types do not match or if the types are not CHANNEL_DOUBLE or
 * CHANNEL_INTEGER, then it returns FALSE.
 */
int ChannelValueGeq(ChannelValue * val1, ChannelValue * val2);

/*
* If both val1 and val2 have the same ChannelType then ChannelValueEq
* returns the respective result for (==).
*
* If the types do not match then it returns FALSE.
*/
int ChannelValueEq(ChannelValue * val1, ChannelValue * val2);

/*
 * If both val and offset have the same ChannelType that is either
 * CHANNEL_DOUBLE or CHANNEL_INTEGER then ChannelValueAddOffset adds
 * offset to val and returns RETURN_OK.
 *
 * If the types do not match or if the types are not CHANNEL_DOUBLE or
 * CHANNEL_INTEGER, then it returns RETURN_ERROR.
 */
McxStatus ChannelValueAddOffset(ChannelValue * val, ChannelValue * offset);

/*
 * If both val and factor have the same ChannelType that is either
 * CHANNEL_DOUBLE or CHANNEL_INTEGER then ChannelValueScale sets val
 * to val * factor and returns RETURN_OK.
 *
 * If the types do not match or if the types are not CHANNEL_DOUBLE or
 * CHANNEL_INTEGER, then it returns RETURN_ERROR.
 */
McxStatus ChannelValueScale(ChannelValue * val, ChannelValue * factor);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CHANNELS_CHANNEL_VALUE_H */