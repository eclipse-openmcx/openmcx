/********************************************************************************
 * Copyright (c) 2022 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_OBJECTS_VECTOR_H
#define MCX_OBJECTS_VECTOR_H

#include "common/status.h"
#include "objects/Object.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



typedef struct Vector Vector;

typedef McxStatus (*fVectorElemInitializer)(void * elem);
typedef McxStatus (*fVectorElemSetter)(void * elem, void * other);
typedef void (*fVectorElemDestructor)(void * elem);
typedef void (*fVectorSetup)(Vector * vector, size_t elemSize, fVectorElemInitializer elemInitializer,
                             fVectorElemSetter elemSetter, fVectorElemDestructor elemDestructor);
typedef size_t (*fVectorSize)(const Vector * vector);
typedef int (*fVectorElemPredicate)(const void * elem, const void * args);
typedef void * (*fVectorFind)(const Vector * vector, fVectorElemPredicate pred, const void * args);
typedef size_t (*fVectorFindIdx)(const Vector * vector, fVectorElemPredicate pred, const void * args);
typedef void * (*fVectorAt)(const Vector * vector, size_t idx);
typedef McxStatus (*fVectorSetAt)(Vector * vector, size_t pos, void * elem);
typedef McxStatus (*fVectorReserve)(Vector * vector, size_t newCapacity);
typedef McxStatus (*fVectorPushBack)(Vector * vector, void * elem);
typedef McxStatus (*fVectorAppend)(Vector * vector, Vector * appendee);
typedef Vector * (*fVectorFilter)(const Vector * vector, fVectorElemPredicate predicate, void * args);
typedef Vector * (*fVectorFilterRef)(const Vector * vector, fVectorElemPredicate predicate, void * args);
typedef int (*fVectorContains)(Vector * vector, void * elem);

extern const struct ObjectClass _Vector;


typedef struct Vector {
    Object _;

    fVectorSetup Setup;
    fVectorSize Size;
    fVectorAt At;
    fVectorSetAt SetAt;
    fVectorReserve Reserve;
    fVectorPushBack PushBack;
    fVectorAppend Append;
    fVectorFilter Filter;
    fVectorFilterRef FilterRef;
    fVectorFind Find;
    fVectorFindIdx FindIdx;
    fVectorContains Contains;


    void * elements_;

    size_t elemSize_;
    fVectorElemInitializer elemInitializer_;
    fVectorElemSetter elemSetter_;
    fVectorElemDestructor elemDestructor_;

    size_t size_;
    size_t capacity_;
    size_t increment_;
} Vector;



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_OBJECTS_VECTOR_H */