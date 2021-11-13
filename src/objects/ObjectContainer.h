/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_OBJECTS_OBJECT_CONTAINER_H
#define MCX_OBJECTS_OBJECT_CONTAINER_H

#include "CentralParts.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ObjectContainer ObjectContainer;
struct StringContainer;

typedef int (* fObjectPredicate)(Object * obj);
typedef int (* fObjectPredicateCtx)(Object * obj, void * ctx);
typedef void (* fObjectIter)(Object * obj);

typedef size_t (* fObjectContainerSize)(const ObjectContainer * container);
typedef McxStatus (* fObjectContainerResize)(ObjectContainer * container, size_t size);
typedef McxStatus (* fObjectContainerPushBack)(ObjectContainer  * container, Object * obj);
typedef McxStatus (* fObjectContainerPushBackNamed)(ObjectContainer  * container, Object * obj, const char * name);
typedef Object * (* fObjectContainerAt)(const ObjectContainer * container, size_t pos);
typedef Object * (* fObjectContainerGetByName)(const ObjectContainer * container, const char * name);
typedef McxStatus (* fObjectContainerSetAt)(ObjectContainer * container, size_t pos, Object * obj);
typedef ObjectContainer * (* fObjectContainerCopy)(ObjectContainer * container);
typedef McxStatus (* fObjectContainerAppend)(
    ObjectContainer * container,
    ObjectContainer * appendee);
typedef Object ** (* fObjectContainerData)(ObjectContainer * container);
typedef void (* fObjectContainerAssignArray)(ObjectContainer * container,
                                             size_t            size,
                                             Object         ** objs);
typedef void (* fObjectContainerDestroyObjects)(ObjectContainer * container);
typedef McxStatus (* fObjectContainerSetElementName)(ObjectContainer * container,
                                                     size_t            pos,
                                                     const char      * name);
typedef const char * (* fObjectContainerGetElementName)(ObjectContainer * container, size_t pos);
typedef int (* fObjectContainerGetNameIndex)(ObjectContainer * container,
                                             const char * name);
typedef int (* fObjectContainerContains)(ObjectContainer * container,
                                         Object * obj);
typedef ObjectContainer * (* fObjectContainerFilter)(ObjectContainer * container, fObjectPredicate predicate);
typedef ObjectContainer * (* fObjectContainerFilterCtx)(ObjectContainer * container, fObjectPredicateCtx predicate, void * ctx);

typedef McxStatus (*fObjectContainerSort)(ObjectContainer * container, int (*cmp)(const void *, const void *, void *), void * arg);


typedef void (* fObjectContainerIterate)(ObjectContainer * container, fObjectIter iter);

extern const struct ObjectClass _ObjectContainer;

typedef struct ObjectContainer {
    Object _; /* super class first */

    fObjectContainerSize Size;
    fObjectContainerResize Resize;
    fObjectContainerPushBack PushBack;
    fObjectContainerPushBackNamed PushBackNamed;
    fObjectContainerAt At;
    fObjectContainerGetByName GetByName;
    fObjectContainerSetAt SetAt;
    fObjectContainerCopy Copy;
    fObjectContainerAppend Append;
    fObjectContainerData Data;
    fObjectContainerAssignArray AssignArray;

    fObjectContainerDestroyObjects DestroyObjects;

    fObjectContainerSetElementName SetElementName;
    fObjectContainerGetElementName GetElementName;
    fObjectContainerGetNameIndex GetNameIndex;

    fObjectContainerContains Contains;
    fObjectContainerFilter Filter;
    fObjectContainerFilterCtx FilterCtx;

    fObjectContainerSort Sort;

    fObjectContainerIterate Iterate;

    struct Object ** elements;
    size_t size;
    size_t capacity;
    size_t increment;
    struct StringContainer * strToIdx;
} ObjectContainer;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_OBJECTS_OBJECT_CONTAINER_H */