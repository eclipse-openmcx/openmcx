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

typedef struct ObjectList ObjectList;

typedef int (* fObjectPredicate)(Object * obj);
typedef int (* fObjectPredicateCtx)(Object * obj, void * ctx);
typedef void (* fObjectIter)(Object * obj);

typedef size_t(*fObjectListSize)(const ObjectList * container);
typedef McxStatus(*fObjectListResize)(ObjectList * container, size_t size);
typedef McxStatus(*fObjectListPushBack)(ObjectList  * container, Object * obj);
typedef Object * (*fObjectListAt)(const ObjectList * container, size_t pos);
typedef McxStatus(*fObjectListSetAt)(ObjectList * container, size_t pos, Object * obj);
typedef ObjectList * (*fObjectListCopy)(ObjectList * container);
typedef McxStatus(*fObjectListAppend)(ObjectList * container, ObjectList * appendee);
typedef Object ** (*fObjectListData)(ObjectList * container);
typedef void (*fObjectListAssignArray)(ObjectList * container, size_t size, Object** objs);
typedef void (*fObjectListDestroyObjects)(ObjectList * container);
typedef int (*fObjectListContains)(ObjectList * container, Object * obj);
typedef ObjectList * (*fObjectListFilter)(ObjectList * container, fObjectPredicate predicate);
typedef ObjectList * (*fObjectListFilterCtx)(ObjectList * container, fObjectPredicateCtx predicate, void * ctx);

typedef McxStatus(*fObjectListSort)(ObjectList * container, int (*cmp)(const void *, const void *, void *), void * arg);
typedef void (*fObjectListIterate)(ObjectList * container, fObjectIter iter);

extern const struct ObjectClass _ObjectList;

typedef struct ObjectList {
    Object _; /* super class first */

    fObjectListSize Size;
    fObjectListResize Resize;
    fObjectListPushBack PushBack;
    fObjectListAt At;
    fObjectListSetAt SetAt;
    fObjectListCopy Copy;
    fObjectListAppend Append;
    fObjectListData Data;
    fObjectListAssignArray AssignArray;

    fObjectListDestroyObjects DestroyObjects;

    fObjectListContains Contains;
    fObjectListFilter Filter;
    fObjectListFilterCtx FilterCtx;

    fObjectListSort Sort;
    fObjectListIterate Iterate;

    struct Object ** elements;
    size_t size;
    size_t capacity;
    size_t increment;
} ObjectList;

typedef struct ObjectContainer ObjectContainer;
struct StringContainer;

typedef size_t(*fObjectContainerSize)(const ObjectContainer * container);
typedef McxStatus(*fObjectContainerResize)(ObjectContainer * container, size_t size);
typedef McxStatus(*fObjectContainerPushBack)(ObjectContainer  * container, Object * obj);
typedef Object * (*fObjectContainerAt)(const ObjectContainer * container, size_t pos);
typedef McxStatus(*fObjectContainerSetAt)(ObjectContainer * container, size_t pos, Object * obj);
typedef Object * (*fObjectContainerObjectCopyCallback)(Object * obj);
typedef void (*fObjectContainerRegisterObjectCopyCallback)(ObjectContainer * container, fObjectContainerObjectCopyCallback callback);
typedef ObjectContainer * (*fObjectContainerCopy)(ObjectContainer * container);
typedef McxStatus(*fObjectContainerAppend)(ObjectContainer * container, ObjectContainer * appendee);
typedef Object ** (*fObjectContainerData)(ObjectContainer * container);
typedef void (*fObjectContainerAssignArray)(ObjectContainer * container, size_t size, Object** objs);
typedef void (*fObjectContainerDestroyObjects)(ObjectContainer * container);
typedef int (*fObjectContainerContains)(ObjectContainer * container, Object * obj);
typedef ObjectContainer * (*fObjectContainerFilter)(ObjectContainer * container, fObjectPredicate predicate);
typedef ObjectContainer * (*fObjectContainerFilterCtx)(ObjectContainer * container, fObjectPredicateCtx predicate, void * ctx);

typedef McxStatus(*fObjectContainerSort)(ObjectContainer * container, int (*cmp)(const void *, const void *, void *), void * arg);
typedef void (*fObjectContainerIterate)(ObjectContainer * container, fObjectIter iter);


typedef McxStatus(*fObjectContainerSetElementName)(ObjectContainer * container, size_t pos, const char * name);
typedef int (*fObjectContainerGetNameIndex)(ObjectContainer * container, const char * name);
typedef McxStatus(*fObjectContainerPushBackNamed)(ObjectContainer  * container, Object * obj, const char * name);
typedef Object * (*fObjectContainerGetByName)(const ObjectContainer * container, const char * name);
typedef const char * (*fObjectContainerGetElementName)(ObjectContainer * container, size_t pos);


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
    fObjectContainerRegisterObjectCopyCallback RegisterObjectCopyCallback;
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

    fObjectContainerObjectCopyCallback ObjectCopyCallback;

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