/********************************************************************************
 * Copyright (c) 2022 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "objects/Vector.h"

#include "common/logging.h"
#include "common/memory.h"

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



static size_t VectorSize(const Vector * vector) {
    return vector->size_;
}

static void * VectorAt(const Vector * vector, size_t idx) {
    if (idx < vector->size_) {
        char * it = (char *) vector->elements_;
        return (void *) (it + idx * vector->elemSize_);
    } else {
        return NULL;
    }
}

static size_t VectorFindIdx(const Vector * vector, fVectorElemPredicate pred, void * args) {
    size_t i = 0;
    char * it = NULL;

    for (i = 0, it = (char*) vector->elements_; i < vector->size_; ++i, it += vector->elemSize_) {
        if (pred((void *) it, args)) {
            return i;
        }
    }

    return SIZE_T_ERROR;
}

static void * VectorFind(const Vector * vector, fVectorElemPredicate pred, void * args) {
    size_t idx = vector->FindIdx(vector, pred, args);

    if (idx == SIZE_T_ERROR) {
        return NULL;
    }

    return vector->At(vector, idx);
}

static McxStatus VectorResize(Vector * vector, size_t size) {
    size_t oldSize = vector->size_;
    size_t oldCapacity = vector->capacity_;
    size_t i = 0;
    McxStatus retVal = RETURN_OK;

    vector->size_ = size;
    if (oldCapacity < size) {
        vector->capacity_ = size + vector->increment_;
        vector->elements_ = mcx_realloc(vector->elements_, vector->capacity_ * vector->elemSize_);
        if (!vector->elements_) {
            mcx_log(LOG_ERROR, "Vector: Resize: Memory allocation failed");
            return RETURN_ERROR;
        }
    }

    // if we make the vector larger, init new elements
    if (vector->elemInitializer_) {
        for (i = oldSize; i < size; ++i) {
            retVal = vector->elemInitializer_(vector->At(vector, i));
            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "Vector: Resize: Element initialization failed");
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus VectorPushBack(Vector * vector, void * elem) {
    McxStatus retVal = vector->Resize(vector, vector->size_ + 1);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Vector: PushBack: Resize failed");
        return RETURN_ERROR;
    }

    if (vector->elemSetter_) {
        retVal = vector->elemSetter_(vector->At(vector, vector->size_ - 1), elem);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Vector: PushBack: Element setting failed");
            return RETURN_ERROR;
        }
    } else {
        memcpy(vector->At(vector, vector->size_ - 1), elem, vector->elemSize_);
    }

    return RETURN_OK;
}

static McxStatus VectorAppend(Vector * vector, Vector * appendee) {
    size_t appendeeSize = 0;
    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    if (!appendee) {
        mcx_log(LOG_ERROR, "Vector: Append: Appendee missing");
        return RETURN_ERROR;
    }

    appendeeSize = appendee->Size(appendee);
    for (i = 0; i < appendeeSize; i++) {
        retVal = vector->PushBack(vector, appendee->At(appendee, i));
        if (RETURN_OK != retVal) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static Vector * VectorFilter(const Vector * vector, fVectorElemPredicate predicate, void * args) {
    Vector * filtered = (Vector *) object_create(Vector);
    size_t i = 0;
    char * it = NULL;

    if (!filtered) {
        mcx_log(LOG_ERROR, "Vector: Filter: Not enough memory");
        return NULL;
    }

    filtered->Setup(filtered, vector->elemSize_, vector->elemInitializer_, vector->elemSetter_, vector->elemDestructor_);

    for (i = 0, it = (char*)vector->elements_; i < vector->size_; ++i, it += vector->elemSize_) {
        if (predicate((void *) it, args)) {
            filtered->PushBack(filtered, (void *) it);
        }
    }

    return filtered;
}

static Vector * VectorFilterRef(const Vector * vector, fVectorElemPredicate predicate, void * args) {
    Vector * filtered = (Vector *) object_create(Vector);
    size_t i = 0;
    char * it = NULL;

    if (!filtered) {
        mcx_log(LOG_ERROR, "Vector: FilterReferences: Not enough memory");
        return NULL;
    }

    filtered->Setup(filtered, sizeof(void *), NULL, NULL, NULL);

    for (i = 0, it = (char*)vector->elements_; i < vector->size_; ++i, it += vector->elemSize_) {
        if (predicate((void *) it, args)) {
            filtered->PushBack(filtered, (void *) &it);
        }
    }

    return filtered;
}


static int VectorContains(Vector * vector, void * elem) {
    size_t i = 0;

    for (i = 0; i < vector->size_; i++) {
        if (vector->At(vector, i) == elem) {
            return 1;
        }
    }

    return 0;
}

static void VectorSetup(
    Vector * vector,
    size_t elemSize,
    fVectorElemInitializer elemInitializer,
    fVectorElemSetter elemSetter,
    fVectorElemDestructor elemDestructor)
{
    vector->elemSize_ = elemSize;
    vector->elemInitializer_ = elemInitializer;
    vector->elemSetter_ = elemSetter;
    vector->elemDestructor_ = elemDestructor;
}

static void VectorDestructor(Vector * vector) {
    if (vector->elements_) {
        if (vector->elemDestructor_) {
            size_t i = 0;
            char * it = (char *)vector->elements_;

            for (i = 0; i < vector->size_; ++i) {
                vector->elemDestructor_((void *) (it + i * vector->elemSize_));
            }
        }

        mcx_free(vector->elements_);
    }
}

static Vector * VectorCreate(Vector * vector) {
    vector->Setup = VectorSetup;
    vector->Size = VectorSize;
    vector->At = VectorAt;
    vector->Resize = VectorResize;
    vector->PushBack = VectorPushBack;
    vector->Append = VectorAppend;
    vector->Filter = VectorFilter;
    vector->FilterRef = VectorFilterRef;
    vector->Find = VectorFind;
    vector->FindIdx = VectorFindIdx;
    vector->Contains = VectorContains;

    vector->elements_ = NULL;

    vector->elemSize_ = 0;
    vector->elemDestructor_ = NULL;
    vector->elemInitializer_ = NULL;
    vector->elemSetter_ = NULL;

    vector->size_ = 0;
    vector->capacity_ = 0;
    vector->increment_ = 10;

    return vector;
}

OBJECT_CLASS(Vector, Object);



#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */