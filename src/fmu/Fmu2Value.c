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
#include "core/connections/ConnectionInfoFactory.h"
#include "CentralParts.h"
#include "util/string.h"
#include "util/stdlib.h"
#include "fmu/common_fmu2.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void Fmu2ValueDataDestructor(Fmu2ValueData * data) {
}

static Fmu2ValueData * Fmu2ValueDataCreate(Fmu2ValueData * data) {
    memset(data, 0, sizeof(Fmu2ValueData));

    data->type = FMU2_VALUE_INVALID;

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
                info->type = &ChannelTypeDouble;
                min.d = fmi2_import_get_real_variable_min(fmi2_import_get_variable_as_real(var));
                minDefined = min.d != -DBL_MAX;
                max.d = fmi2_import_get_real_variable_max(fmi2_import_get_variable_as_real(var));
                maxDefined = max.d != DBL_MAX;
                break;
            case fmi2_base_type_int:
                info->type = &ChannelTypeInteger;
                min.i = fmi2_import_get_integer_variable_min(fmi2_import_get_variable_as_integer(var));
                minDefined = min.i != -INT_MIN;
                max.i = fmi2_import_get_integer_variable_max(fmi2_import_get_variable_as_integer(var));
                maxDefined = max.i != INT_MAX;
                break;
            case fmi2_base_type_enum:
                info->type = &ChannelTypeInteger;
                min.i = fmi2_import_get_enum_variable_min(fmi2_import_get_variable_as_enum(var));
                minDefined = min.i != -INT_MIN;
                max.i = fmi2_import_get_enum_variable_max(fmi2_import_get_variable_as_enum(var));
                maxDefined = max.i != INT_MAX;
                break;
            case fmi2_base_type_str:
                info->type = &ChannelTypeString;
                break;
            case fmi2_base_type_bool:
                info->type = &ChannelTypeBool;
                break;
            default:
                info->type = &ChannelTypeUnknown;
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
    info->type = &ChannelTypeUnknown;

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

Fmu2ValueData * Fmu2ValueDataArrayMake(size_t numDims, size_t dims[], fmi2_import_variable_t ** values) {
    Fmu2ValueData * data = NULL;

    if (!numDims) {
        return NULL;
    }

    data = (Fmu2ValueData *) object_create(Fmu2ValueData);
    if (data) {
        size_t num = 1;
        size_t i = 0;

        for (i = 0; i < numDims; i++) {
            num *= dims[i];
        }

        data->type = FMU2_VALUE_ARRAY;
        data->data.array.numDims = numDims;
        data->data.array.dims = mcx_copy(dims, sizeof(size_t) * numDims);
        if (!data->data.array.dims) { goto error; }

        data->data.array.values = mcx_copy(values, num * sizeof(fmi2_import_variable_t *));
        if (!data->data.array.values) { goto error; }

        data->vr.array.values = mcx_calloc(num, sizeof(fmi2_value_reference_t));
        if (!data->vr.array.values) { goto error; }

        for (i = 0; i < num; i++) {
            data->vr.array.values[i] = fmi2_import_get_variable_vr(data->data.array.values[i]);
        }
    }

    return data;
error:
    if (data) { object_destroy(data); }
    return NULL;
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

static McxStatus Fmu2ValueGetVariableStart(fmi2_base_type_enu_t t, fmi2_import_variable_t * var, ChannelValue * value) {

    switch (t) {
    case fmi2_base_type_real:
        value->value.d = fmi2_import_get_real_variable_start(fmi2_import_get_variable_as_real(var));
        break;
    case fmi2_base_type_int:
        value->value.i = fmi2_import_get_integer_variable_start(fmi2_import_get_variable_as_integer(var));
        break;
    case fmi2_base_type_bool:
        value->value.i = fmi2_import_get_boolean_variable_start(fmi2_import_get_variable_as_boolean(var));
        break;
    case fmi2_base_type_str: {
        const char * buffer = fmi2_import_get_string_variable_start(fmi2_import_get_variable_as_string(var));
        if (RETURN_OK != ChannelValueSetFromReference(value, &buffer)) {
            return RETURN_ERROR;
        }
        break;
    }
    case fmi2_base_type_enum:
        value->value.i = fmi2_import_get_enum_variable_start(fmi2_import_get_variable_as_enum(var));
        break;
    default:
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Base type %s not supported", fmi2_base_type_to_string(t));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus Fmu2ValueGetArrayVariableStart(fmi2_base_type_enu_t t, fmi2_import_variable_t * var, array * a, size_t i) {

    switch (t) {
    case fmi2_base_type_real:
        ((double *)a->data)[i] = fmi2_import_get_real_variable_start(fmi2_import_get_variable_as_real(var));
        break;
    case fmi2_base_type_int:
        ((int *)a->data)[i] = fmi2_import_get_integer_variable_start(fmi2_import_get_variable_as_integer(var));
        break;
    case fmi2_base_type_enum:
       ((int *)a->data)[i] = fmi2_import_get_enum_variable_start(fmi2_import_get_variable_as_enum(var));
        break;
    case fmi2_base_type_bool:
        ((int *)a->data)[i] = fmi2_import_get_boolean_variable_start(fmi2_import_get_variable_as_boolean(var));
        break;
    default:
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Array base type %s not supported", fmi2_base_type_to_string(t));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus Fmu2ValueGetBinaryVariableStart(fmi2_import_variable_t * varHi, fmi2_import_variable_t * varLo, fmi2_import_variable_t * varSize, ChannelValue * value) {
    fmi2_base_type_enu_t t;

    t = fmi2_import_get_variable_base_type(varHi);
    if (t != fmi2_base_type_int) {
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Binary base type (hi) %s not supported", fmi2_base_type_to_string(t));
        return RETURN_ERROR;
    }
    t = fmi2_import_get_variable_base_type(varLo);
    if (t != fmi2_base_type_int) {
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Binary base type (lo) %s not supported", fmi2_base_type_to_string(t));
        return RETURN_ERROR;
    }
    t = fmi2_import_get_variable_base_type(varSize);
    if (t != fmi2_base_type_int) {
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Binary base type (size) %s not supported", fmi2_base_type_to_string(t));
        return RETURN_ERROR;
    }

    fmi2_integer_t hi = fmi2_import_get_integer_variable_start(fmi2_import_get_variable_as_integer(varHi));
    fmi2_integer_t lo = fmi2_import_get_integer_variable_start(fmi2_import_get_variable_as_integer(varLo));
    fmi2_integer_t size = fmi2_import_get_integer_variable_start(fmi2_import_get_variable_as_integer(varSize));

    binary_string b;

    b.len = size;
    b.data = (char *) ((((long long)hi & 0xffffffff) << 32) | (lo & 0xffffffff));

    if (RETURN_OK != ChannelValueSetFromReference(value, &b)) {
        mcx_log(LOG_ERROR, "Fmu2Value: Could not set value");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}


static McxStatus Fmu2ValueSetup(Fmu2Value * v, const char * name, Fmu2ValueData * data, const char * unit, Channel * channel) {
    if (!name || !data) {
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Name or data missing");
        return RETURN_ERROR;
    }

    v->name = mcx_string_copy(name);
    v->unit = mcx_string_copy(unit);
    v->data = data;
    v->channel = channel;

    if (!v->name) {
        mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Cannot copy name");
        return RETURN_ERROR;
    }

    if (v->data->type == FMU2_VALUE_SCALAR) {
        fmi2_base_type_enu_t t = fmi2_import_get_variable_base_type(data->data.scalar);

        ChannelValueInit(&v->val, ChannelTypeClone(Fmi2TypeToChannelType(t)));


        if (RETURN_OK != Fmu2ValueGetVariableStart(t, data->data.scalar, &v->val)) {
            return RETURN_ERROR;
        }
    } else if (v->data->type == FMU2_VALUE_ARRAY) {
        fmi2_base_type_enu_t t = fmi2_import_get_variable_base_type(data->data.array.values[0]);

        ChannelValueInit(&v->val, ChannelTypeArray(Fmi2TypeToChannelType(t), data->data.array.numDims, data->data.array.dims));

        if (data->data.array.numDims == 0) {
            mcx_log(LOG_ERROR, "Fmu2Value: Setup failed: Array of dimension 0 is not supported");
            return RETURN_ERROR;
        }

        size_t i = 0, n = 1;

        for (i = 0; i < data->data.array.numDims; i++) {
            n *= data->data.array.dims[i];
        }

        array * a = (array *) ChannelValueReference(&v->val);

        for (i = 0; i < n; i++) {
            if (RETURN_OK != Fmu2ValueGetArrayVariableStart(t, data->data.array.values[i], a, i)) {
                return RETURN_ERROR;
            }
        }
    } else if (v->data->type == FMU2_VALUE_BINARY_OSI) {
        return RETURN_OK;

        // TODO: Setting the initial value causes some memory errors.

        // ChannelValueInit(&v->val, &ChannelTypeBinary);

        // if (RETURN_OK != Fmu2ValueGetBinaryVariableStart(
        //         data->data.binary.hi,
        //         data->data.binary.lo,
        //         data->data.binary.size,
        //         &v->val)) {
        //     return RETURN_ERROR;
        // }


    } else {
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

    ChannelValueInit(&v->val, ChannelTypeClone(&ChannelTypeUnknown));

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

Fmu2Value * Fmu2ValueArrayMake(const char * name, size_t numDims, size_t dims[], fmi2_import_variable_t ** values, const char * unit, Channel * channel) {
    Fmu2ValueData * data = Fmu2ValueDataArrayMake(numDims, dims, values);
    Fmu2Value * value = Fmu2ValueMake(name, data, unit, channel);

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

Fmu2Value * Fmu2ReadFmu2ScalarValue(const char * logPrefix, ChannelType * type, const char * channelName, const char * unitString, fmi2_import_t * fmiImport) {
    Fmu2Value * val = NULL;
    fmi2_import_variable_t * var = NULL;

    var = fmi2_import_get_variable_by_name(fmiImport, channelName);
    if (!var) {
        mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix, channelName);
        return NULL;
    }

    if (!ChannelTypeEq(type, Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var)))) {
        mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix, channelName);
        mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s", logPrefix,
                ChannelTypeToString(type),
                ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var))));
        return NULL;
    }

    val = Fmu2ValueScalarMake(channelName, var, unitString, NULL);
    if (!val) {
        mcx_log(LOG_ERROR, "%s: Could not set value for channel %s", logPrefix, channelName);
        return NULL;
    }

    return val;
}

Fmu2Value * Fmu2ReadFmu2ArrayValue(const char * logPrefix, ChannelType * type, const char * channelName, ChannelDimension * dimension, const char * unitString, fmi2_import_t * fmiImport) {
    Fmu2Value * val = NULL;
    fmi2_import_variable_t * var = NULL;

    if (dimension->num > 1) {
        mcx_log(LOG_ERROR, "%s: Port %s: Invalid dimension", logPrefix, channelName);
        return RETURN_ERROR;
    }

    size_t i = 0;
    size_t startIdx = dimension->startIdxs[0];
    size_t endIdx = dimension->endIdxs[0];

    fmi2_import_variable_t ** vars = mcx_calloc(endIdx - startIdx + 1, sizeof(fmi2_import_variable_t *));
    if (!vars) {
        return NULL;
    }

    for (i = startIdx; i <= endIdx; i++) {
        char * indexedChannelName = CreateIndexedName(channelName, i);
        fmi2_import_variable_t * var = fmi2_import_get_variable_by_name(fmiImport, indexedChannelName);
        if (!var) {
            mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix, indexedChannelName);
            return NULL;
        }
        if (!ChannelTypeEq(ChannelTypeArrayInner(type), Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var)))) {
            mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix, indexedChannelName);
            mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s", logPrefix,
                    ChannelTypeToString(ChannelTypeArrayInner(type)),
                    ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var))));
            return NULL;
        }
        vars[i - startIdx] = var;

        mcx_free(indexedChannelName);
    }

    size_t dims[] = { endIdx - startIdx + 1 };
    val = Fmu2ValueArrayMake(channelName, 1 /* numDims */, dims, vars, unitString, NULL);
    if (!val) {
        mcx_log(LOG_ERROR, "%s: Could not set value for channel %s", logPrefix, channelName);
        return NULL;
    }

    return val;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */