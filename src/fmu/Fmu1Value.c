/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "fmu/Fmu1Value.h"

#include "fmu/common_fmu1.h"

#include "util/string.h"
#include "util/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static void Fmu1ValueDataDestructor(Fmu1ValueData * data) {
    if (data->type == FMU1_VALUE_ARRAY) {
        if (data->var.array.dims) {
            mcx_free(data->var.array.dims);
        }
        if (data->var.array.values) {
            mcx_free(data->var.array.values);
        }
        data->var.array.numDims = 0;

        if (data->vr.array.values) {
            mcx_free(data->vr.array.values);
        }
    }

    data->type = FMU1_VALUE_INVALID;
}

static Fmu1ValueData * Fmu1ValueDataCreate(Fmu1ValueData * data) {
    memset(data, 0, sizeof(Fmu1ValueData));

    data->type = FMU1_VALUE_INVALID;

    return data;
}

OBJECT_CLASS(Fmu1ValueData, Object);


size_t Fmu1ValueDataArrayNumElems(const Fmu1ValueData * data) {
    size_t i = 0;
    size_t num = 1;

    if (data->type != FMU1_VALUE_ARRAY) {
        return 0;
    }

    for (i = 0; i < data->var.array.numDims; i++) {
        num *= data->var.array.dims[i];
    }

    return num;
}


static Fmu1ValueData * Fmu1ValueDataScalarMake(fmi1_import_variable_t * scalar) {
    Fmu1ValueData * data = NULL;

    if (!scalar) {
        return NULL;
    }

    data = (Fmu1ValueData *) object_create(Fmu1ValueData);
    if (data) {
        data->type = FMU1_VALUE_SCALAR;
        data->var.scalar = scalar;
        data->vr.scalar = fmi1_import_get_variable_vr(scalar);
    }

    return data;
}

static Fmu1ValueData * Fmu1ValueDataArrayMake(size_t numDims, size_t dims[], fmi1_import_variable_t ** values) {
    Fmu1ValueData * data = NULL;

    if (numDims == 0) {
        return NULL;
    }

    data = (Fmu1ValueData *) object_create(Fmu1ValueData);
    if (data) {
        size_t i = 0;
        size_t num = 1;

        for (i = 0; i < numDims; i++) {
            num *= dims[i];
        }

        data->type = FMU1_VALUE_ARRAY;

        data->var.array.numDims = numDims;
        data->var.array.dims = mcx_copy(dims, sizeof(size_t) * numDims);
        if (!data->var.array.dims) {
            goto cleanup;
        }

        data->var.array.values = mcx_copy(values, sizeof(fmi1_import_variable_t *) * num);
        if (!data->var.array.values) {
            goto cleanup;
        }

        data->vr.array.values = mcx_calloc(num, sizeof(fmi1_value_reference_t));
        if (!data->vr.array.values) {
            goto cleanup;
        }

        for (i = 0; i < num; i++) {
            data->vr.array.values[i] = fmi2_import_get_variable_vr(data->var.array.values[i]);
        }
    }
    return data;

cleanup:
    object_destroy(data);
    return NULL;
}


static McxStatus Fmu1ValueSetFromChannelValue(Fmu1Value * v, ChannelValue * val) {
    return ChannelValueSet(&v->val, val);
}

static McxStatus Fmu1ValueSetup(Fmu1Value * value, const char * name, Fmu1ValueData * data, Channel * channel) {
    if (!name || !data) {
        mcx_log(LOG_ERROR, "Fmu1Value: Setup failed: Name or data missing");
        return RETURN_ERROR;
    }

    value->name = mcx_string_copy(name);
    value->data = data;
    value->channel = channel;

    if (!value->name) {
        mcx_log(LOG_ERROR, "Fmu1Value: Setup failed: Can not copy name");
        return RETURN_ERROR;
    }

    switch (data->type) {
        case FMU1_VALUE_SCALAR:
            {
                fmi1_base_type_enu_t t = fmi1_import_get_variable_base_type(data->var.scalar);
                ChannelValueInit(&value->val, ChannelTypeClone(Fmi1TypeToChannelType(t)));
                break;
            }
        case FMU1_VALUE_ARRAY:
            {
                fmi1_base_type_enu_t t = fmi1_import_get_variable_base_type(data->var.array.values[0]);
                ChannelValueInit(&value->val, ChannelTypeArray(Fmi1TypeToChannelType(t), data->var.array.numDims, data->var.array.dims));
                break;
            }
        default:
            mcx_log(LOG_ERROR, "Fmu1Value: Setup failed: Invalid data type");
            return RETURN_ERROR;
    }

    return RETURN_OK;
}

void Fmu1ValueDestructor(Fmu1Value * v) {
    if (v->name) {
        mcx_free(v->name);
        v->name = NULL;
    }

    object_destroy(v->data);

    ChannelValueDestructor(&v->val);
}

Fmu1Value * Fmu1ValueCreate(Fmu1Value * v) {
    v->SetFromChannelValue = Fmu1ValueSetFromChannelValue;
    v->Setup = Fmu1ValueSetup;

    v->name = NULL;
    v->data = NULL;
    v->channel = NULL;
    ChannelValueInit(&v->val, ChannelTypeClone(&ChannelTypeUnknown));

    return v;
}

OBJECT_CLASS(Fmu1Value, Object);


static Fmu1Value * Fmu1ValueMake(const char * name, Fmu1ValueData * data, Channel * channel) {
    Fmu1Value * value = (Fmu1Value *)object_create(Fmu1Value);

    if (value) {
        if (RETURN_OK != value->Setup(value, name, data, channel)) {
            object_destroy(value);
            return NULL;
        }
    }

    return value;
}

Fmu1Value * Fmu1ValueScalarMake(const char * name, fmi1_import_variable_t * var, Channel * channel) {
    Fmu1ValueData * data = Fmu1ValueDataScalarMake(var);
    return Fmu1ValueMake(name, data, channel);
}

Fmu1Value * Fmu1ValueArrayMake(const char * name, size_t numDims, size_t * dims, fmi1_import_variable_t ** vars, Channel * channel) {
    Fmu1ValueData * data = Fmu1ValueDataArrayMake(numDims, dims, vars);
    return Fmu1ValueMake(name, data, channel);
}

Fmu1Value * Fmu1ValueReadScalar(const char * logPrefix, ChannelType * type, Channel * channel, const char * channelName, fmi1_import_t * fmiImport) {
    Fmu1Value * value = NULL;
    fmi1_import_variable_t * var = NULL;

    var = fmi1_import_get_variable_by_name(fmiImport, channelName);
    if (!var) {
        mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix, channelName);
        return NULL;
    }

    if (!ChannelTypeEq(type, Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var)))) {
        mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix, channelName);
        mcx_log(LOG_ERROR,
                "%s: Expected: %s, Imported from FMU: %s",
                logPrefix,
                ChannelTypeToString(type),
                ChannelTypeToString(Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var))));
        return NULL;
    }

    value = Fmu1ValueScalarMake(channelName, var, channel);
    if (!value) {
        mcx_log(LOG_ERROR, "%s: Could not set value for channel %s", logPrefix, channelName);
        return RETURN_ERROR;
    }

    return value;
}

Fmu1Value * Fmu1ValueReadArray(const char * logPrefix, ChannelType * type, Channel * channel, const char * channelName, ChannelDimension * dimension, fmi1_import_t * fmiImport) {
    Fmu1Value * value = NULL;
    fmi1_import_variable_t * var = NULL;

    if (dimension->num > 1) {
        mcx_log(LOG_ERROR, "%s: Port %s: Invalid dimension", logPrefix, channelName);
        goto cleanup;
    }

    size_t i = 0;
    size_t startIdx = dimension->startIdxs[0];
    size_t endIdx = dimension->endIdxs[0];

    fmi1_import_variable_t ** vars = mcx_calloc(endIdx - startIdx + 1, sizeof(fmi1_import_variable_t *));
    if (!vars) {
        goto cleanup;
    }

    for (i = startIdx; i <= endIdx; i++) {
        char * indexedChannelName = CreateIndexedName(channelName, (unsigned int) i);
        fmi1_import_variable_t * var = fmi1_import_get_variable_by_name(fmiImport, indexedChannelName);
        if (!var) {
            mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix, indexedChannelName);
            goto cleanup;
        }
        if (!ChannelTypeEq(ChannelTypeArrayInner(type), Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var)))) {
            mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix, indexedChannelName);
            mcx_log(LOG_ERROR,
                    "%s: Expected: %s, Imported from FMU: %s",
                    logPrefix,
                    ChannelTypeToString(ChannelTypeArrayInner(type)),
                    ChannelTypeToString(Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var))));
            goto cleanup;
        }
        vars[i - startIdx] = var;

        mcx_free(indexedChannelName);
    }

    size_t dims[] = {endIdx - startIdx + 1};
    value = Fmu1ValueArrayMake(channelName, 1 /* numDims */, dims, vars, channel);
    if (!value) {
        mcx_log(LOG_ERROR, "%s: Could not set value for channel %s", logPrefix, channelName);
        goto cleanup;
    }

cleanup:
    if (vars) {
        mcx_free(vars);
    }

    return value;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */