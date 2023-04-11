/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/model/ports/ScalarPortInput.h"

#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static InputElement * Clone(InputElement * self) {
    ScalarPortInput * clone = (ScalarPortInput *)object_create(ScalarPortInput);
    McxStatus retVal = RETURN_ERROR;

    if (!clone) {
        return NULL;
    }

    retVal = clone->CopyFrom(clone, (ScalarPortInput *) self);

    // cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(clone);
        return NULL;
    }

    return (InputElement *) clone;
}

static McxStatus CopyFrom(ScalarPortInput * self, ScalarPortInput * src) {
    InputElement * base = (InputElement *)self;
    McxStatus retVal = RETURN_OK;

    if (!self || !src) {
        mcx_log(LOG_ERROR, "ScalarPortInput: Invalid copy arguments");
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

    self->min.defined = src->min.defined;
    if (self->min.defined) {
        ChannelValueDataInit(&self->min.value, self->type);
        if (RETURN_OK != ChannelValueDataSetFromReference(&self->min.value, self->type, &src->min.value)) {
            return RETURN_ERROR;
        }
    }

    self->max.defined = src->max.defined;
    if (self->max.defined) {
        ChannelValueDataInit(&self->max.value, self->type);
        if (RETURN_OK != ChannelValueDataSetFromReference(&self->max.value, self->type, &src->max.value)) {
            return RETURN_ERROR;
        }
    }

    self->scale.defined = src->scale.defined;
    if (self->scale.defined) {
        ChannelValueDataInit(&self->scale.value, self->type);
        if (RETURN_OK != ChannelValueDataSetFromReference(&self->scale.value, self->type, &src->scale.value)) {
            return RETURN_ERROR;
        }
    }

    self->offset.defined = src->offset.defined;
    if (self->offset.defined) {
        ChannelValueDataInit(&self->offset.value, self->type);
        if (RETURN_OK != ChannelValueDataSetFromReference(&self->offset.value, self->type, &src->offset.value)) {
            return RETURN_ERROR;
        }
    }

    self->default_.defined = src->default_.defined;
    if (self->default_.defined) {
        ChannelValueDataInit(&self->default_.value, self->type);
        if (RETURN_OK != ChannelValueDataSetFromReference(&self->default_.value, self->type, &src->default_.value)) {
            return RETURN_ERROR;
        }
    }

    self->initial.defined = src->initial.defined;
    if (self->initial.defined) {
        ChannelValueDataInit(&self->initial.value, self->type);
        if (RETURN_OK != ChannelValueDataSetFromReference(&self->initial.value, self->type, &src->initial.value)) {
            return RETURN_ERROR;
        }
    }

    self->writeResults = src->writeResults;

    return RETURN_OK;
}


static void PrintOptionalChannelValueData(char * prefix, ChannelType * type, OPTIONAL_VALUE(ChannelValueData) value) {
    switch(type->con) {
    case CHANNEL_DOUBLE:
        if (value.defined) {
            mcx_log(LOG_DEBUG, "%s%f,", prefix, value.value.d);
        } else {
            mcx_log(LOG_DEBUG, "%s-,", prefix);
        }
        break;
    case CHANNEL_BOOL:
    case CHANNEL_INTEGER:
        if (value.defined) {
            mcx_log(LOG_DEBUG, "%s%d,", prefix, value.value.i);
        } else {
            mcx_log(LOG_DEBUG, "%s-,", prefix);
        }
        break;
    default:
        break;
    }
}

static void PrintOptionalInt64(char * prefix, OPTIONAL_VALUE(int64_t) value) {
    if (value.defined) {
        mcx_log(LOG_DEBUG, "%s%lld,", prefix, value.value);
    } else {
        mcx_log(LOG_DEBUG, "%s-,", prefix);
    }
}

void ScalarPortInputPrint(ScalarPortInput * input) {
    mcx_log(LOG_DEBUG, "ScalarPortInput @ %#x {", input);
    mcx_log(LOG_DEBUG, "  .name: %s,", input->name);
    mcx_log(LOG_DEBUG, "  .nameInModel: %s,", input->nameInModel);
    mcx_log(LOG_DEBUG, "  .description: %s,", input->description);
    mcx_log(LOG_DEBUG, "  .id: %s,", input->id);
    mcx_log(LOG_DEBUG, "  .unit: %s,", input->unit);
    mcx_log(LOG_DEBUG, "  .type: %d,", input->type);

    PrintOptionalChannelValueData("  .min: ", input->type, input->min);
    PrintOptionalChannelValueData("  .max: ", input->type, input->max);
    PrintOptionalChannelValueData("  .scale: ", input->type, input->scale);
    PrintOptionalChannelValueData("  .offset: ", input->type, input->offset);
    PrintOptionalChannelValueData("  .default: ", input->type, input->default_);
    PrintOptionalChannelValueData("  .initial: ", input->type, input->initial);

    PrintOptionalInt64("  .writeResults: ", input->writeResults);
    mcx_log(LOG_DEBUG, "}");
}

static void ScalarPortInputDestructor(ScalarPortInput * input) {
    if (input->name) { mcx_free(input->name); }
    if (input->nameInModel) { mcx_free(input->nameInModel); }
    if (input->description) { mcx_free(input->description); }
    if (input->id) { mcx_free(input->id); }
    if (input->unit) { mcx_free(input->unit); }

    if (input->min.defined) { ChannelValueDataDestructor(&input->min.value, input->type); }
    if (input->max.defined) { ChannelValueDataDestructor(&input->max.value, input->type); }

    if (input->scale.defined) { ChannelValueDataDestructor(&input->scale.value, input->type); }
    if (input->offset.defined) { ChannelValueDataDestructor(&input->offset.value, input->type); }

    if (input->default_.defined) { ChannelValueDataDestructor(&input->default_.value, input->type); }
    if (input->initial.defined) { ChannelValueDataDestructor(&input->initial.value, input->type); }
}

static ScalarPortInput * ScalarPortInputCreate(ScalarPortInput * input) {
    InputElement * inputElement = (InputElement *)input;

    input->name = NULL;
    input->nameInModel = NULL;
    input->description = NULL;
    input->id = NULL;
    input->unit = NULL;

    input->type = &ChannelTypeUnknown;

    OPTIONAL_UNSET(input->min);
    OPTIONAL_UNSET(input->max);

    OPTIONAL_UNSET(input->scale);
    OPTIONAL_UNSET(input->offset);

    OPTIONAL_UNSET(input->default_);
    OPTIONAL_UNSET(input->initial);

    OPTIONAL_UNSET(input->writeResults);

    inputElement->Clone = Clone;
    input->CopyFrom = CopyFrom;

    return input;
}

OBJECT_CLASS(ScalarPortInput, InputElement);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */