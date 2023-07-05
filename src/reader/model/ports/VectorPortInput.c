/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/model/ports/VectorPortInput.h"

#include "channel_value/ChannelValue.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static InputElement * Clone(InputElement * self) {
    VectorPortInput * clone = (VectorPortInput *)object_create(VectorPortInput);
    McxStatus retVal = RETURN_ERROR;

    if (!clone) {
        return NULL;
    }

    retVal = clone->CopyFrom(clone, (VectorPortInput *) self);

    // cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(clone);
        return NULL;
    }

    return (InputElement *) clone;
}

static McxStatus CopyFrom(VectorPortInput * self, VectorPortInput * src) {
    InputElement * base = (InputElement *)self;
    McxStatus retVal = RETURN_OK;

    if (!self || !src) {
        mcx_log(LOG_ERROR, "VectorPortInput: Invalid copy arguments");
        return RETURN_ERROR;
    }

    // copy base class first
    retVal = base->CopyFrom(base, (InputElement *)src);
    if (retVal == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    // copy src members
    self->name = mcx_string_copy(src->name);
    if (!self->name && src->name) {
        return RETURN_ERROR;
    }

    self->nameInModel = mcx_string_copy(src->nameInModel);
    if (!self->nameInModel && src->nameInModel) {
        return RETURN_ERROR;
    }

    self->description = mcx_string_copy(src->description);
    if (!self->description && src->description) {
        return RETURN_ERROR;
    }

    self->id = mcx_string_copy(src->id);
    if (!self->id && src->id) {
        return RETURN_ERROR;
    }

    self->unit = mcx_string_copy(src->unit);
    if (!self->unit && src->unit) {
        return RETURN_ERROR;
    }

    self->type = src->type;
    self->startIndex = src->startIndex;
    self->endIndex = src->endIndex;

    size_t len = (size_t) (self->endIndex - self->startIndex + 1);
    size_t size = ChannelValueTypeSize(self->type);

    if (src->min) {
        self->min = mcx_calloc(len, size);
        if (!self->min) {
            return RETURN_ERROR;
        }
        memcpy(self->min, src->min, len * size);
    } else {
        self->min = NULL;
    }

    if (src->max) {
        self->max = mcx_calloc(len, size);
        if (!self->max) {
            return RETURN_ERROR;
        }
        memcpy(self->max, src->max, len * size);
    } else {
        self->max = NULL;
    }

    if (src->scale) {
        self->scale = mcx_calloc(len, size);
        if (!self->scale) {
            return RETURN_ERROR;
        }
        memcpy(self->scale, src->scale, len * size);
    } else {
        self->max = NULL;
    }

    if (src->offset) {
        self->offset = mcx_calloc(len, size);
        if (!self->offset) {
            return RETURN_ERROR;
        }
        memcpy(self->offset, src->offset, len * size);
    } else {
        self->offset = NULL;
    }

    if (src->default_) {
        self->default_ = mcx_calloc(len, size);
        if (!self->default_) {
            return RETURN_ERROR;
        }
        memcpy(self->default_, src->default_, len * size);
    } else {
        self->default_ = NULL;
    }

    if (src->initial) {
        self->initial = mcx_calloc(len, size);
        if (!self->initial) {
            return RETURN_ERROR;
        }
        memcpy(self->initial, src->initial, len * size);
    } else {
        self->default_ = NULL;
    }

    self->writeResults = src->writeResults;

    return RETURN_OK;
}

static void PrintVec(char * prefix, ChannelType * type, size_t len, void * value) {
    char buffer[4096] = { 0 };

    size_t num = 0;
    size_t i = 0;

    num += sprintf(buffer + num, "%s", prefix);

    if (value) {
        for (i = 0; i < len; i++) {
            switch(type->con) {
            case CHANNEL_DOUBLE:
                num += sprintf(buffer + num, " %f", ((double*)value)[i]);
                break;
            case CHANNEL_BOOL:
            case CHANNEL_INTEGER:
                num += sprintf(buffer + num, " %d", ((int*)value)[i]);
                break;
            default:
                break;
            }
        }
    } else {
        num += sprintf(buffer + num, "-");
    }
    num += sprintf(buffer + num, "%s", ",");

    mcx_log(LOG_DEBUG, "%s", buffer);
}

static void PrintOptionalInt32(char * prefix, OPTIONAL_VALUE(int) value) {
    if (value.defined) {
        mcx_log(LOG_DEBUG, "%s%d,", prefix, value.value);
    } else {
        mcx_log(LOG_DEBUG, "%s-,", prefix);
    }
}

static void PrintOptionalInt64(char * prefix, OPTIONAL_VALUE(int64_t) value) {
    if (value.defined) {
        mcx_log(LOG_DEBUG, "%s%lld,", prefix, value.value);
    } else {
        mcx_log(LOG_DEBUG, "%s-,", prefix);
    }
}

void VectorPortInputPrint(VectorPortInput * input) {
    size_t len = (size_t) (input->endIndex - input->startIndex + 1);

    mcx_log(LOG_DEBUG, "VectorPortInput @ %#x {", input);
    mcx_log(LOG_DEBUG, "  .name: %s,", input->name);
    mcx_log(LOG_DEBUG, "  .nameInModel: %s,", input->nameInModel);
    mcx_log(LOG_DEBUG, "  .description: %s,", input->description);
    mcx_log(LOG_DEBUG, "  .id: %s,", input->id);
    mcx_log(LOG_DEBUG, "  .unit: %s,", input->unit);
    mcx_log(LOG_DEBUG, "  .type: %d,", input->type);

    PrintVec("  .min: ", input->type, len, input->min);
    PrintVec("  .max: ", input->type, len, input->max);
    PrintVec("  .scale: ", input->type, len, input->scale);
    PrintVec("  .offset: ", input->type, len, input->offset);
    PrintVec("  .default: ", input->type, len, input->default_);
    PrintVec("  .initial: ", input->type, len, input->initial);

    PrintOptionalInt32("  .writeResults: ", input->writeResults);
    mcx_log(LOG_DEBUG, "}");
}

static void VectorPortInputDestructor(VectorPortInput * input) {
    if (input->name) { mcx_free(input->name); }
    if (input->nameInModel) { mcx_free(input->nameInModel); }
    if (input->description) { mcx_free(input->description); }
    if (input->id) { mcx_free(input->id); }
    if (input->unit) { mcx_free(input->unit); }
    if (input->min) { mcx_free(input->min); }
    if (input->max) { mcx_free(input->max); }
    if (input->scale) { mcx_free(input->scale); }
    if (input->offset) { mcx_free(input->offset); }
    if (input->default_) { mcx_free(input->default_); }
    if (input->initial) { mcx_free(input->initial); }
}

static VectorPortInput * VectorPortInputCreate(VectorPortInput * input) {
    InputElement * inputElement = (InputElement *)input;

    input->startIndex = -1;
    input->endIndex = -1;

    input->name = NULL;
    input->nameInModel = NULL;
    input->description = NULL;
    input->id = NULL;
    input->unit = NULL;

    input->type = &ChannelTypeUnknown;

    input->min = NULL;
    input->max = NULL;

    input->scale = NULL;
    input->offset = NULL;

    input->default_ = NULL;
    input->initial = NULL;

    OPTIONAL_UNSET(input->writeResults);

    inputElement->Clone = Clone;
    input->CopyFrom = CopyFrom;

    return input;
}

OBJECT_CLASS(VectorPortInput, InputElement);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */