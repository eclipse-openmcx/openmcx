/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "fmu/Fmu2Value.h"

#include "core/channels/ChannelValue.h"
#include "CentralParts.h"
#include "util/string.h"
#include "fmu/common_fmu2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void Fmu2ValueDataDestructor(Fmu2ValueData * data) {
}

static Fmu2ValueData * Fmu2ValueDataCreate(Fmu2ValueData * data) {
    data->type = FMU2_VALUE_INVALID;

    data->data.binary.lo = NULL;
    data->data.binary.hi = NULL;
    data->data.binary.size = NULL;

    return data;
}

OBJECT_CLASS(Fmu2ValueData, Object);

Fmu2VariableInfo * Fmu2VariableInfoMake(fmi2_import_variable_t * var) {
    Fmu2VariableInfo * info = (Fmu2VariableInfo *)object_create(Fmu2VariableInfo);

    if (info) {
        fmi2_base_type_enu_t type = fmi2_import_get_variable_base_type(var);
        ChannelValueData min = { 0 };
        int minDefined = FALSE;
        ChannelValueData max = { 0 };
        int maxDefined = FALSE;

        char * xmlDesc = fmi2_import_get_variable_description(var);
        info->desc = mcx_string_copy(xmlDesc);
        if (xmlDesc && !info->desc) {
            goto cleanup;
        }

        switch (type) {
            case fmi2_base_type_real:
                info->type = ChannelTypeDouble;
                min.d = fmi2_import_get_real_variable_min(fmi2_import_get_variable_as_real(var));
                minDefined = min.d != -DBL_MAX;
                max.d = fmi2_import_get_real_variable_max(fmi2_import_get_variable_as_real(var));
                maxDefined = max.d != DBL_MAX;
                break;
            case fmi2_base_type_int:
                info->type = ChannelTypeInteger;
                min.i = fmi2_import_get_integer_variable_min(fmi2_import_get_variable_as_integer(var));
                minDefined = min.i != -INT_MIN;
                max.i = fmi2_import_get_integer_variable_max(fmi2_import_get_variable_as_integer(var));
                maxDefined = max.i != INT_MAX;
                break;
            case fmi2_base_type_enum:
                info->type = ChannelTypeInteger;
                min.i = fmi2_import_get_enum_variable_min(fmi2_import_get_variable_as_enum(var));
                minDefined = min.i != -INT_MIN;
                max.i = fmi2_import_get_enum_variable_max(fmi2_import_get_variable_as_enum(var));
                maxDefined = max.i != INT_MAX;
                break;
            case fmi2_base_type_str:
                info->type = ChannelTypeString;
                break;
            case fmi2_base_type_bool:
                info->type = ChannelTypeBool;
                break;
            default:
                info->type = ChannelTypeUnknown;
                break;
        }

        if (minDefined) {
            info->min = (ChannelValueData *)mcx_calloc(1, sizeof(ChannelValueData));
            if (!info->min) {
                goto cleanup;
            }
            ChannelValueDataSetFromReference(info->min, info->type, &min);
        }

        if (maxDefined) {
            info->max = (ChannelValueData*)mcx_calloc(1, sizeof(ChannelValueData));
            if (!info->max) {
                goto cleanup;
            }
            ChannelValueDataSetFromReference(info->max, info->type, &max);
        }
    }

    return info;

cleanup:
    object_destroy(info);
    return NULL;
}

static void Fmu2VariableInfoDestructor(Fmu2VariableInfo * info) {
    if (info->min) {
        ChannelValueDataDestructor(info->min, info->type);
        mcx_free(info->min);
    }
    if (info->max) {
        ChannelValueDataDestructor(info->max, info->type);
        mcx_free(info->max);
    }

    if (info->desc) { mcx_free(info->desc); }
}

static Fmu2VariableInfo * Fmu2VariableInfoCreate(Fmu2VariableInfo * info) {
    info->type = ChannelTypeUnknown;

    info->min = NULL;
    info->max = NULL;

    info->desc = NULL;

    return info;
}

OBJECT_CLASS(Fmu2VariableInfo, Object);


Fmu2ValueData * Fmu2ValueDataScalarMake(fmi2_import_variable_t * scalar) {
    Fmu2ValueData * data = (Fmu2ValueData *) object_create(Fmu2ValueData);

    if (data) {
        if (!scalar) {
            object_destroy(data);
            return NULL;
        }
        data->type = FMU2_VALUE_SCALAR;
        data->data.scalar = scalar;
        data->vr.scalar = fmi2_import_get_variable_vr(scalar);
    }

    return data;
}

Fmu2ValueData * Fmu2ValueDataBinaryMake(fmi2_import_variable_t * hi, fmi2_import_variable_t * lo, fmi2_import_variable_t * size) {
    Fmu2ValueData * data = (Fmu2ValueData *) object_create(Fmu2ValueData);

    if (data) {
        if (!hi || !lo || !size) {
            object_destroy(data);
            return NULL;
        }

        data->type = FMU2_VALUE_BINARY_OSI;
        data->data.binary.hi = hi;
        data->data.binary.lo = lo;
        data->data.binary.size = size;
        data->vr.binary.hi = fmi2_import_get_variable_vr(hi);
        data->vr.binary.lo = fmi2_import_get_variable_vr(lo);
        data->vr.binary.size = fmi2_import_get_variable_vr(size);
    }

    return data;
}

static McxStatus Fmu2ValueSetFromChannelValue(Fmu2Value * v, ChannelValue * val) {
    return ChannelValueSet(&v->val, val);
}

static McxStatus Fmu2ValueSetup(Fmu2Value * v, const char * name, Fmu2ValueData * data, const char * unit, Channel * channel) {
    fmi2_base_type_enu_t t;

    if (!name || !data) {
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Name or data missing");
        return RETURN_ERROR;
    }

    t = fmi2_import_get_variable_base_type(data->data.scalar);

    v->name = mcx_string_copy(name);
    v->unit = mcx_string_copy(unit);
    v->data = data;
    v->channel = channel;
    ChannelValueInit(&v->val, Fmi2TypeToChannelType(t));

    if (!v->name) {
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Cannot copy name");
        return RETURN_ERROR;
    }

    switch (t) {
    case fmi2_base_type_real:
        v->val.value.d = fmi2_import_get_real_variable_start(fmi2_import_get_variable_as_real(data->data.scalar));
        break;
    case fmi2_base_type_int:
        v->val.value.i = fmi2_import_get_integer_variable_start(fmi2_import_get_variable_as_integer(data->data.scalar));
        break;
    case fmi2_base_type_bool:
        v->val.value.i = fmi2_import_get_boolean_variable_start(fmi2_import_get_variable_as_boolean(data->data.scalar));
        break;
    case fmi2_base_type_str: {
        const char * buffer = fmi2_import_get_string_variable_start(fmi2_import_get_variable_as_string(data->data.scalar));
        if (RETURN_OK != ChannelValueSetFromReference(&v->val, &buffer)) {
            return RETURN_ERROR;
        }
        break;
    }
    case fmi2_base_type_enum:
        v->val.value.i = fmi2_import_get_enum_variable_start(fmi2_import_get_variable_as_enum(data->data.scalar));
        break;
    default:
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Base type %s not supported", fmi2_base_type_to_string(t));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void Fmu2ValueSetChannel(Fmu2Value * v, Channel * channel) {
    v->channel = channel;
}

static void Fmu2ValueDestructor(Fmu2Value * v) {
    if (v->name) {
        mcx_free(v->name);
        v->name = NULL;
    }
    if (v->unit) {
        mcx_free(v->unit);
        v->unit = NULL;
    }
    object_destroy(v->data);
    object_destroy(v->info);
    ChannelValueDestructor(&v->val);
}

static Fmu2Value * Fmu2ValueCreate(Fmu2Value * v) {
    v->Setup = Fmu2ValueSetup;
    v->SetFromChannelValue = Fmu2ValueSetFromChannelValue;
    v->SetChannel = Fmu2ValueSetChannel;

    v->name = NULL;
    v->unit = NULL;
    v->data = NULL;
    v->channel = NULL;
    v->info = NULL;

    ChannelValueInit(&v->val, ChannelTypeUnknown);

    return v;
}

OBJECT_CLASS(Fmu2Value, Object);

Fmu2Value * Fmu2ValueMake(const char * name, Fmu2ValueData * data, const char * unit, Channel * channel) {
    Fmu2Value * value = (Fmu2Value *) object_create(Fmu2Value);

    if (value) {
        McxStatus retVal = RETURN_OK;
        retVal = value->Setup(value, name, data, unit, channel);
        if (RETURN_OK != retVal) {
            object_destroy(value);
            return NULL;
        }
    }

    return value;
}

Fmu2Value * Fmu2ValueScalarMake(const char * name, fmi2_import_variable_t * scalar, const char * unit, Channel * channel) {
    Fmu2ValueData * data = Fmu2ValueDataScalarMake(scalar);
    Fmu2Value * value = Fmu2ValueMake(name, data, unit, channel);

    value->info = Fmu2VariableInfoMake(scalar);

    return value;
}

Fmu2Value * Fmu2ValueBinaryMake(const char * name, fmi2_import_variable_t * hi, fmi2_import_variable_t * lo, fmi2_import_variable_t * size, Channel * channel) {
    Fmu2ValueData * data = Fmu2ValueDataBinaryMake(hi, lo, size);
    Fmu2Value * value = Fmu2ValueMake(name, data, NULL, channel);

    return value;
}

void Fmu2ValuePrintDebug(Fmu2Value * val) {
    mcx_log(LOG_DEBUG, "Fmu2Value { name: \"%s\" }", val->name);
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */