/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "objects/ObjectContainer.h"
#include "objects/StringContainer.h"
#include "CentralParts.h"
#include "util/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ObjectContainerElement {
    Object * object;
    char * name;
    void * value;
} ObjectContainerElement;


static size_t ObjectContainerSize(const ObjectContainer * container) {
    return container->size;
}

static McxStatus ObjectContainerResize(ObjectContainer * container, size_t size) {
    size_t oldSize = container->size;
    size_t i = 0;

    container->size     = size;
    container->elements = (Object * *) mcx_realloc(container->elements,
                                     size * sizeof(Object *));
    if (!container->elements && 0 < size) {
        mcx_log(LOG_ERROR, "ObjectContainer: Resize: Memory allocation failed");
        return RETURN_ERROR;
    }

    /* if we make the container larger, init new elements with NULL */
    for (i = oldSize; i < size; i++) {
        container->elements[i] = NULL;
    }

    return StringContainerResize(container->strToIdx, container->size);
}

static McxStatus ObjectContainerPushBack(ObjectContainer * container, Object * obj) {
    container->size  += 1;
    container->elements = (Object * *) mcx_realloc(container->elements,
                                     container->size * sizeof(Object *));
    if (!container->elements) {
        mcx_log(LOG_ERROR, "ObjectContainer: PushBack: Memory allocation failed");
        return RETURN_ERROR;
    }

    container->elements[container->size - 1] = obj;

    return StringContainerResize(container->strToIdx, container->size);
}

static McxStatus ObjectContainerPushBackNamed(ObjectContainer  * container, Object * obj, const char * name) {
    McxStatus retVal = RETURN_OK;

    retVal = container->PushBack(container, obj);
    if (RETURN_OK != retVal) {
        return RETURN_ERROR;
    }
    retVal = StringContainerSetString(container->strToIdx, container->size - 1, name);
    if (RETURN_OK != retVal) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

typedef struct {
    int (*cmp)(const void *, const void *, void *);
    void * arg;
} StrCmpCtx;

#if(__APPLE__)
static int ObjectContainerElementCmp(void *ctx, const void *first,
                                     const void *second) {
#else
static int ObjectContainerElementCmp(const void * first, const void * second,
                                     void * ctx) {
#endif
    StrCmpCtx *data = (StrCmpCtx *)ctx;

    ObjectContainerElement * firstElement = (ObjectContainerElement *) first;
    ObjectContainerElement * secondElement = (ObjectContainerElement *) second;

    return data->cmp(&(firstElement->object), &(secondElement->object), data->arg);
}

static McxStatus ObjectContainerSort(ObjectContainer * container, int (*cmp)(const void *, const void *, void *), void * arg) {
    size_t i = 0;
    size_t n = container->size;

    StrCmpCtx ctx;
    ctx.cmp = cmp;
    ctx.arg = arg;

    ObjectContainerElement * elements = mcx_malloc(n * sizeof(ObjectContainerElement));
    if (!elements) {
        return RETURN_ERROR;
    }

    for (i = 0; i < n; i++) {
        elements[i].object = container->elements[i];
        elements[i].name = container->strToIdx->keys[i];
        elements[i].value = container->strToIdx->values[i];
    }

    mcx_sort(elements, n, sizeof(ObjectContainerElement), ObjectContainerElementCmp, &ctx);

    for (i = 0; i < n; i++) {
        container->elements[i] = elements[i].object;
        container->strToIdx->keys[i] = elements[i].name;
        container->strToIdx->values[i] = elements[i].value;
    }

    mcx_free(elements);

    return RETURN_OK;
}

static Object * ObjectContainerAt(const ObjectContainer * container, size_t pos) {
    if (pos < container->size) {
        return container->elements[pos];
    } else {
        return NULL;
    }
}

static McxStatus ObjectContainerSetAt(ObjectContainer * container, size_t pos, Object * obj) {
    if (pos >= container->size) {
        container->Resize(container, pos + 1);
    }

    container->elements[pos] = obj;

    return RETURN_OK;
}

static ObjectContainer * ObjectContainerCopy(ObjectContainer * container) {
    McxStatus retVal;
    size_t i = 0;

    ObjectContainer * newContainer = (ObjectContainer *) object_create(ObjectContainer);

    if (!newContainer) {
        mcx_log(LOG_ERROR, "ObjectContainer: Copy: Memory allocation failed");
        return NULL;
    }

    retVal = newContainer->Resize(newContainer, container->Size(container));
    if (RETURN_OK != retVal) {
        object_destroy(newContainer);
        return NULL;
    }

    for (i = 0; i < newContainer->Size(newContainer); i++) {
        newContainer->elements[i] = container->elements[i];
        retVal = StringContainerSetString(newContainer->strToIdx, i,
                                          StringContainerGetString(container->strToIdx, i));
        if (retVal != RETURN_OK) {
            return NULL;
        }
    }

    return newContainer;
}

static McxStatus ObjectContainerAppend(
    ObjectContainer * container,
    ObjectContainer * appendee) {
    size_t appendeeSize = 0;
    size_t size = 0;
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    if (!appendee) {
        mcx_log(LOG_ERROR, "ObjectContainer: Append: Appendee missing");
        return RETURN_ERROR;
    }

    appendeeSize = appendee->Size(appendee);
    for (i = 0; i < appendeeSize; i++) {
        retVal = container->PushBack(container, appendee->At(appendee, i));
        if (RETURN_OK != retVal) {
            return RETURN_ERROR;
        }

        retVal = container->SetElementName(container, container->Size(container) - 1,
                                           appendee->GetElementName(appendee, i));
        if (RETURN_OK != retVal) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static Object ** ObjectContainerData(ObjectContainer * container) {
    return container->elements;
}

static void ObjectContainerAssignArray(ObjectContainer * container,
                                       size_t            size,
                                       Object         ** objs) {
    container->size = size;

    if (container->elements) {
        mcx_free(container->elements);
    }
    container->elements = objs;
}

static void ObjectContainerDestroyObjects(ObjectContainer * container) {
    size_t i = 0;

    for (i = 0; i < container->size; i++) {
        Object * obj = container->At(container, i);
        object_destroy(obj);
    }

    container->Resize(container, 0);
}

static McxStatus ObjectContainerSetElementName(ObjectContainer * container,
                                               size_t            pos,
                                               const char      * name) {
    if (pos >= container->size) {
        mcx_log(LOG_ERROR, "ObjectContainer: SetElementName: Position %u out of bounds, max is %u", pos, container->size);
        return RETURN_ERROR;
    }

    return StringContainerSetString(container->strToIdx, pos, name);
}

static const char * ObjectContainerGetElementName(ObjectContainer * container,
                                                  size_t pos) {
    return StringContainerGetString(container->strToIdx, pos);
}

static int ObjectContainerGetNameIndex(ObjectContainer * container,
                                             const char * name) {
    return StringContainerGetIndex(container->strToIdx, name);
}

static Object * ObjectContainerGetByName(const ObjectContainer * container, const char * name) {
    return container->At(container, container->GetNameIndex((ObjectContainer *)container, name));
}
static int ObjectContainerContains(ObjectContainer * container,
                                   Object * obj) {
    size_t i = 0;
    for (i = 0; i < container->Size(container); i++) {
        if (container->At(container, i) == obj) {
            return TRUE;
        }
    }
    return FALSE;
}

static ObjectContainer * ObjectContainerFilter(ObjectContainer * container, fObjectPredicate predicate) {
    ObjectContainer * filtered = (ObjectContainer *) object_create(ObjectContainer);

    size_t i = 0;

    for (i = 0; i < container->Size(container); i++) {
        Object * obj = container->At(container, i);
        if (predicate(obj)) {
            filtered->PushBack(filtered, obj);
        }
    }

    return filtered;
}

static ObjectContainer * ObjectContainerFilterCtx(ObjectContainer * container, fObjectPredicateCtx predicate, void * ctx) {
    ObjectContainer * filtered = (ObjectContainer *) object_create(ObjectContainer);

    size_t i = 0;

    for (i = 0; i < container->Size(container); i++) {
        Object * obj = container->At(container, i);
        if (predicate(obj, ctx)) {
            filtered->PushBack(filtered, obj);
        }
    }

    return filtered;
}

static void ObjectContainerIterate(ObjectContainer * container, fObjectIter iter) {
    size_t i = 0;
    for (i = 0; i < container->Size(container); i++) {
        Object * obj = container->At(container, i);
        iter(obj);
    }
}

static void ObjectContainerDestructor(ObjectContainer * container) {
    StringContainerDestroy(container->strToIdx);
    mcx_free(container->strToIdx);

    if (container->elements) {
        mcx_free(container->elements);
    }
}

static ObjectContainer * ObjectContainerCreate(ObjectContainer * container) {
    container->Size = ObjectContainerSize;
    container->Resize = ObjectContainerResize;
    container->PushBack = ObjectContainerPushBack;
    container->PushBackNamed = ObjectContainerPushBackNamed;
    container->At = ObjectContainerAt;
    container->GetByName = ObjectContainerGetByName;
    container->SetAt = ObjectContainerSetAt;
    container->Copy = ObjectContainerCopy;
    container->Append = ObjectContainerAppend;
    container->Data = ObjectContainerData;
    container->AssignArray = ObjectContainerAssignArray;

    container->DestroyObjects = ObjectContainerDestroyObjects;

    container->Contains = ObjectContainerContains;
    container->Filter = ObjectContainerFilter;
    container->FilterCtx = ObjectContainerFilterCtx;

    container->Iterate = ObjectContainerIterate;
    container->Sort = ObjectContainerSort;

    container->SetElementName = ObjectContainerSetElementName;
    container->GetElementName = ObjectContainerGetElementName;
    container->GetNameIndex = ObjectContainerGetNameIndex;

    container->elements = NULL;
    container->size = 0;

    container->strToIdx = (StringContainer *) mcx_malloc(sizeof(StringContainer));
    if (!container->strToIdx) { return NULL; }

    StringContainerInit(container->strToIdx, 0);

    return container;
}

OBJECT_CLASS(ObjectContainer, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */