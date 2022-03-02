/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "fmu/common_fmu2.h"
#include "fmu/common_fmu.h" /* for jm callbacks */

#include "fmu/Fmu2Value.h"

#include "core/channels/ChannelInfo.h"

#include "reader/model/parameters/ArrayParameterDimensionInput.h"
#include "reader/model/parameters/ParameterInput.h"
#include "reader/model/parameters/ParametersInput.h"

#include "util/string.h"
#include "util/stdlib.h"

#include "fmilib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


fmi2_base_type_enu_t ChannelTypeToFmi2Type(ChannelType type) {
    switch (type) {
    case CHANNEL_DOUBLE:
        return fmi2_base_type_real;
    case CHANNEL_INTEGER:
        return fmi2_base_type_int;
    case CHANNEL_BOOL:
        return fmi2_base_type_bool;
    case CHANNEL_STRING:
        return fmi2_base_type_str;
    default:
        return fmi2_base_type_real;
    }
}

ChannelType Fmi2TypeToChannelType(fmi2_base_type_enu_t type) {
    switch (type) {
    case fmi2_base_type_real:
        return CHANNEL_DOUBLE;
    case fmi2_base_type_int:
        return CHANNEL_INTEGER;
    case fmi2_base_type_bool:
        return CHANNEL_BOOL;
    case fmi2_base_type_str:
        return CHANNEL_STRING;
    case fmi2_base_type_enum:
        return CHANNEL_INTEGER;
    default:
        return CHANNEL_UNKNOWN;
    }
}

McxStatus Fmu2CommonStructInit(Fmu2CommonStruct * fmu) {
    fmu->fmiImport = NULL;

    fmu->instantiateOk = fmi2_false;
    fmu->runOk = fmi2_false;
    fmu->isLogging = fmi2_false;

    fmu->in = (ObjectContainer *) object_create(ObjectContainer);
    fmu->out = (ObjectContainer *) object_create(ObjectContainer);
    fmu->params = (ObjectContainer *) object_create(ObjectContainer);
    fmu->localValues = (ObjectContainer *) object_create(ObjectContainer);
    fmu->tunableParams = (ObjectContainer *) object_create(ObjectContainer);
    fmu->initialValues = (ObjectContainer *) object_create(ObjectContainer);

    fmu->connectedIn = (ObjectContainer *) object_create(ObjectContainer);

    fmu->numLogCategories = 0;
    fmu->logCategories = NULL;

    return RETURN_OK;
}

int Fmu2ValueIsContainedInObjectContainerPred(Object* obj, void* ctx) {
    Fmu2Value * filterVal = (Fmu2Value *) obj;
    ObjectContainer * vals = (ObjectContainer *) ctx;

    size_t i = 0;
    size_t numVars = vals->Size(vals);

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < numVars; i++) {
        Fmu2Value * const fmuVal = (Fmu2Value *) vals->At(vals, i);

        if (!strcmp(fmuVal->name, filterVal->name)) {
            // vals contains filterVal
            return 1;
        }
    }

    return 0;
}

int Fmu2ValueIsNotContainedInObjectContainerPred(Object* obj, void* ctx) {
    return !Fmu2ValueIsContainedInObjectContainerPred(obj, ctx);
}

McxStatus Fmu2CommonStructRead(FmuCommon * common, Fmu2CommonStruct * fmu2, fmi2_type_t fmu_type, FmuInput * fmuInput) {
    int logging = 0;
    McxStatus retVal = RETURN_OK;

    fmu2->isLogging = fmuInput->isLogging.defined && fmuInput->isLogging.value ? fmi2_true : fmi2_false;

    if (fmuInput->numLogCategories > 0) {
        size_t i = 0;

        fmu2->numLogCategories = fmuInput->numLogCategories;
        fmu2->logCategories = (fmi2_string_t *) mcx_calloc(fmu2->numLogCategories, sizeof(fmi2_string_t));

        for (i = 0; i < fmu2->numLogCategories; i++) {
            fmu2->logCategories[i] = mcx_string_copy(fmuInput->logCategories[i]);
            if (!fmu2->logCategories[i]) {
                return RETURN_ERROR;
            }
        }
    }

    if (!common->instanceName) {
        mcx_log(LOG_ERROR, "FMU instance does not have a name");
        return RETURN_ERROR;
    }

    if (common->version != fmi_version_2_0_enu) {
        mcx_log(LOG_ERROR, "%s: FMU Version mismatch", common->instanceName);
        return RETURN_ERROR;
    }

    fmu2->fmiImport = fmi2_import_parse_xml(
        common->context,
        common->path,
        NULL
    );
    if (NULL == fmu2->fmiImport) {
        mcx_log(LOG_ERROR, "%s: creation of fmi import structure failed", common->instanceName);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus Fmu2CommonStructSetup(FmuCommon * common, Fmu2CommonStruct * fmu2, fmi2_type_t fmu_type) {
    McxStatus retVal = RETURN_OK;
    jm_status_enu_t jmStatus = jm_status_success;
    fmi2_status_t fmi2_status = fmi2_status_ok;

    fmi2_fmu_kind_enu_t fmu_kind;

    if (fmu_type == fmi2_cosimulation) {
        fmu_kind = fmi2_fmu_kind_cs;
    }
    else {
        mcx_log(LOG_ERROR, "%s: unknown kind of fmu", common->instanceName);
        return RETURN_ERROR;
    }

    jmStatus = fmi2_import_create_dllfmu(fmu2->fmiImport, fmu_kind,
        NULL /* callbacks (if NULL then jm callbacks are used) */);
    if (jm_status_success != jmStatus) {
        mcx_log(LOG_ERROR, "%s: Could not load FMU dll", common->instanceName);
        return RETURN_ERROR;
    }

    mcx_log(LOG_DEBUG, "%s: instantiatefn: %x", common->instanceName, fmi2_import_instantiate);
    jmStatus = fmi2_import_instantiate(fmu2->fmiImport,
                                       common->instanceName,
                                       fmu_type,
                                       NULL,
                                       fmi2_false /* visible */);
    if (jm_status_error == jmStatus) {
        mcx_log(LOG_ERROR, "%s: Instantiate failed", common->instanceName);
        return RETURN_ERROR;
    } else if (jm_status_warning == jmStatus) {
        mcx_log(LOG_WARNING, "%s: Instantiate returned with a warning", common->instanceName);
        retVal = RETURN_WARNING;
    }
    fmu2->instantiateOk = fmi2_true;

    fmi2_status = fmi2_import_set_debug_logging(fmu2->fmiImport, fmu2->isLogging, fmu2->numLogCategories, fmu2->logCategories);
    if (fmi2_status_fatal == fmi2_status) {
        mcx_log(LOG_ERROR, "%s: Setting FMI log category failed (fatal error)", common->instanceName);
        return RETURN_ERROR;
    } else if (fmi2_status_error == fmi2_status) {
        mcx_log(LOG_WARNING, "%s: Setting FMI log category failed", common->instanceName);
        retVal = RETURN_WARNING;
    } else if (fmi2_status_warning == fmi2_status) {
        mcx_log(LOG_WARNING, "%s: Setting FMI log category returned with a warning", common->instanceName);
        retVal = RETURN_WARNING;
    } else if (fmi2_status_discard == fmi2_status) {
        mcx_log(LOG_WARNING, "%s: Setting FMI log category discarded", common->instanceName);
        retVal = RETURN_WARNING;
    }

    return retVal;
}

void Fmu2CommonStructDestructor(Fmu2CommonStruct * fmu) {
    size_t i = 0;
    if (fmu->in) {
        fmu->in->DestroyObjects(fmu->in);
        object_destroy(fmu->in);
    }

    if (fmu->out) {
        fmu->out->DestroyObjects(fmu->out);
        object_destroy(fmu->out);
    }

    if (fmu->params) {
        fmu->params->DestroyObjects(fmu->params);
        object_destroy(fmu->params);
    }

    if (fmu->initialValues) {
        fmu->initialValues->DestroyObjects(fmu->initialValues);
        object_destroy(fmu->initialValues);
    }

    if (fmu->localValues) {
        fmu->localValues->DestroyObjects(fmu->localValues);
        object_destroy(fmu->localValues);
    }

    if (fmu->tunableParams) {
        fmu->tunableParams->DestroyObjects(fmu->tunableParams);
        object_destroy(fmu->tunableParams);
    }

    if (fmu->connectedIn) {
        object_destroy(fmu->connectedIn);
    }

    if (fmu->fmiImport) {
        fmi2_import_free(fmu->fmiImport);
        fmu->fmiImport = NULL;
    }
}

static int NaturalComp(const void * left, const void * right, void * arg) {
    Fmu2Value ** left_value = (Fmu2Value **)left;
    Fmu2Value ** right_value = (Fmu2Value **)right;

    const char * l = (*left_value)->name;
    const char * r = (*right_value)->name;

    return mcx_natural_sort_cmp(l, r);
}

Fmu2Value * Fmu2ReadParamValue(ScalarParameterInput * input,
                               fmi2_import_t * import) {
    ChannelValue chVal;
    Fmu2Value * val = NULL;
    fmi2_import_variable_t * var = NULL;
    McxStatus retVal = RETURN_OK;

    if (!input->value.defined) {
        mcx_log(LOG_ERROR, "Parameter %s: No value defined", input->name);
        return NULL;
    }

    ChannelValueInit(&chVal, input->type);
    ChannelValueSetFromReference(&chVal, &input->value.value);

    var = fmi2_import_get_variable_by_name(import, input->name);
    if (!var) {
        return NULL;
    }

    val = Fmu2ValueScalarMake(input->name, var, input->unit, NULL);
    if (!val) {
        return NULL;
    }

    retVal = val->SetFromChannelValue(val, &chVal);
    if (RETURN_OK != retVal) {
        object_destroy(val);
        return NULL;
    }

    return val;
}

#define MAX_DIM_DIGITS 9

static ObjectContainer* Fmu2ReadArrayParamValues(const char * name,
                                                 ArrayParameterInput * input,
                                                 fmi2_import_t * import,
                                                 ObjectContainer * params) {
    ObjectContainer * values = NULL;

    void * vals = NULL;
    size_t stringBufferLength = 0;
    size_t j = 0, k = 0, index = 0;
    size_t start1 = 0, start2 = 0, end1 = 0, end2 = 0;

    McxStatus retVal = RETURN_OK;

    values = (ObjectContainer *)object_create(ObjectContainer);
    if (!values) {
        mcx_log(LOG_ERROR, "FMU: Memory allocation of array values container failed");
        retVal = RETURN_ERROR;
        goto fmu2_read_array_param_values_cleanup;
    }

    // buffer length for the name of a single scalar. It covers the "worst" case with
    // 2 dimensions (MAX_DIM_DIGITS). 3 characters are used for [,]. And at the end we need a '\0'.
    stringBufferLength = strlen(name) + MAX_DIM_DIGITS + MAX_DIM_DIGITS + 3 + 1;

    // set loop boundaries
    if (input->numDims >= 1 && input->dims[0]) {
        start1 = input->dims[0]->start;
        end1 = input->dims[0]->end;
    }
    if (input->numDims >= 2 && input->dims[1]) {
        start2 = input->dims[1]->start;
        end2 = input->dims[1]->end;
    }

    for (k = start2; k <= end2; k++) {
        for (j = start1; j <= end1; j++, index++) {
            Fmu2Value * val = NULL;
            char * varName = (char *) mcx_calloc(stringBufferLength, sizeof(char));
            fmi2_import_variable_t * var = NULL;
            ChannelValue chVal;

            if (!varName) {
                retVal = RETURN_ERROR;
                goto fmu2_read_array_param_values_for_cleanup;
            }

            if (input->numDims == 2) {
                snprintf(varName, stringBufferLength, "%s[%zu,%zu]", name, k, j);
            } else {
                snprintf(varName, stringBufferLength, "%s[%zu]", name, j);
            }

            var = fmi2_import_get_variable_by_name(import, varName);
            if (!var) {
                mcx_log(LOG_ERROR, "FMU: Could not get variable %s", varName);
                retVal = RETURN_ERROR;
                goto fmu2_read_array_param_values_for_cleanup;
            }

            val = Fmu2ValueScalarMake(varName, var, NULL, NULL);
            if (!val) {
                retVal = RETURN_ERROR;
                goto fmu2_read_array_param_values_for_cleanup;
            }

            if (input->type == CHANNEL_DOUBLE) {
                ChannelValueInit(&chVal, CHANNEL_DOUBLE);
                ChannelValueSetFromReference(&chVal, &((double *)input->values)[index]);
            } else { // integer
                ChannelValueInit(&chVal, CHANNEL_INTEGER);
                ChannelValueSetFromReference(&chVal, &((int *)input->values)[index]);
            }

            retVal = val->SetFromChannelValue(val, &chVal);
            if (RETURN_OK != retVal) {
                retVal = RETURN_ERROR;
                goto fmu2_read_array_param_values_for_cleanup;
            }

            // store value
            retVal = values->PushBack(values, (Object *)val);
            if (RETURN_OK != retVal) {
                retVal = RETURN_ERROR;
                goto fmu2_read_array_param_values_for_cleanup;
            }

fmu2_read_array_param_values_for_cleanup:
            if (varName) { mcx_free(varName); }

            if (retVal == RETURN_ERROR) {
                if (val) { object_destroy(val); }
                goto fmu2_read_array_param_values_cleanup;
            }
        }
    }

    {
        size_t i = 0;
        size_t n = values->Size(values);

        if (n > 0) {
            McxStatus status = RETURN_OK;
            status = values->Sort(values, NaturalComp, NULL);
            if (RETURN_OK != status) {
                mcx_log(LOG_ERROR, "FMU: Unable to sort parameters");
                retVal = RETURN_ERROR;
                goto fmu2_read_array_param_values_cleanup;
            }

            for (i = 0; i < n - 1; i++) {
                Fmu2Value * a = (Fmu2Value *) values->At(values, i);
                Fmu2Value * b = (Fmu2Value *) values->At(values, i + 1);

                if (! strcmp(a->name, b->name)) {
                    mcx_log(LOG_ERROR, "FMU: Duplicate definition of parameter %s", a->name);
                    retVal = RETURN_ERROR;
                    goto fmu2_read_array_param_values_cleanup;
                }
            }
        }
    }

fmu2_read_array_param_values_cleanup:
    if (vals) { mcx_free(vals); }

    if (retVal == RETURN_ERROR) {
        if (values) {
            values->DestroyObjects(values);
            object_destroy(values);
        }
        return NULL;
    }

    return values;
}

// Reads parameters from the input file (both scalar and array).
//
// Ignores parameters provided via the `ignore` argument.
ObjectContainer * Fmu2ReadParams(ParametersInput * input, fmi2_import_t * import, ObjectContainer * ignore) {
    ObjectContainer * params = (ObjectContainer *) object_create(ObjectContainer);
    ObjectContainer * ret = params;                  // used for unified cleanup via goto

    size_t i = 0;
    size_t num = 0;
    McxStatus retVal = RETURN_OK;

    if (!params) {
        return NULL;
    }

    num = input->parameters->Size(input->parameters);
    for (i = 0; i < num; i++) {
        ParameterInput * parameterInput = (ParameterInput *) input->parameters->At(input->parameters, i);

        char * name = NULL;
        Fmu2Value * val = NULL;
        ObjectContainer * vals = NULL;


        name = mcx_string_copy(parameterInput->parameter.arrayParameter->name);
        if (!name) {
            ret = NULL;
            goto fmu2_read_params_for_cleanup;
        }

        // ignore the parameter if it is in the `ignore` container
        if (ignore && ignore->GetNameIndex(ignore, name) >= 0) {
            goto fmu2_read_params_for_cleanup;
        }

        if (parameterInput->type == PARAMETER_ARRAY) {
            // read parameter dimensions (if any) - should only be defined for array parameters
            if (parameterInput->parameter.arrayParameter->numDims >=2 &&
                parameterInput->parameter.arrayParameter->dims[1] &&
                !parameterInput->parameter.arrayParameter->dims[0]) {
                mcx_log(LOG_ERROR, "FMU: Array parameter %s: Missing definition for the first dimension "
                        "while the second dimension is defined.", parameterInput->parameter.arrayParameter->name);
                ret = NULL;
                goto fmu2_read_params_for_cleanup;
            }

            // array - split it into scalars
            vals = Fmu2ReadArrayParamValues(name, parameterInput->parameter.arrayParameter, import, params);
            if (vals == NULL) {
                mcx_log(LOG_ERROR, "FMU: Could not read array parameter %s", name);
                ret = NULL;
                goto fmu2_read_params_for_cleanup;
            }

            // store the scalar values
            size_t j = 0;
            for (j = 0; j < vals->Size(vals); j++) {
                Object * v = vals->At(vals, j);
                retVal = params->PushBackNamed(params, v, ((Fmu2Value*)v)->name);
                if (RETURN_OK != retVal) {
                    ret = NULL;
                    goto fmu2_read_params_for_cleanup;
                }
            }

            object_destroy(vals);
        } else {
            // read the scalar value
            val = Fmu2ReadParamValue(parameterInput->parameter.scalarParameter, import);
            if (val == NULL) {
                mcx_log(LOG_ERROR, "FMU: Could not read parameter value of parameter %s", name);
                ret = NULL;
                goto fmu2_read_params_for_cleanup;
            }

            // store the read value
            retVal = params->PushBackNamed(params, (Object * ) val, name);
            if (RETURN_OK != retVal) {
                ret = NULL;
                goto fmu2_read_params_for_cleanup;
            }
        }

fmu2_read_params_for_cleanup:
        if (name) { mcx_free(name); }
        if (ret == NULL) {
            if (val)  { object_destroy(val); }
            if (vals) {
                vals->DestroyObjects(vals);
                object_destroy(vals);
            }

            goto cleanup;
        }
    }

    {
        size_t i = 0;
        size_t n = params->Size(params);

        if (n > 0) {
            McxStatus status = RETURN_OK;
            status = params->Sort(params, NaturalComp, NULL);
            if (RETURN_OK != status) {
                mcx_log(LOG_ERROR, "FMU: Unable to sort parameters");
                ret = NULL;
                goto cleanup;
            }

            for (i = 0; i < n - 1; i++) {
                Fmu2Value * a = (Fmu2Value *) params->At(params, i);
                Fmu2Value * b = (Fmu2Value *) params->At(params, i + 1);

                if (! strcmp(a->name, b->name)) {
                    mcx_log(LOG_ERROR, "FMU: Duplicate definition of parameter %s", a->name);
                    ret = NULL;
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (ret == NULL) {
        params->DestroyObjects(params);
        object_destroy(params);
    }

    return ret;
}


McxStatus Fmu2UpdateTunableParamValues(ObjectContainer * tunableParams, ObjectContainer * params) {
    size_t i = 0, j = 0;
    McxStatus retVal = RETURN_OK;

    size_t numTunableParams = tunableParams->Size(tunableParams);
    size_t numParams = params->Size(params);

    if (0 == numParams) {
        return RETURN_OK;
    }

    // the function expects that params are already sorted
    retVal = tunableParams->Sort(tunableParams, NaturalComp, NULL);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "FMU: Unable to sort tunable parameters");
        return RETURN_ERROR;
    }

    for (i = 0, j = 0; i < numTunableParams; i++) {
        Fmu2Value * tunable = (Fmu2Value *)tunableParams->At(tunableParams, i);
        Fmu2Value * param = (Fmu2Value *)params->At(params, j);

        while (NaturalComp(&param, &tunable, NULL) < 0 && j < (numParams-1)) {
            j++;
            param = (Fmu2Value *)params->At(params, j);
        }

        if (0 == NaturalComp(&param, &tunable, NULL)) {
            tunable->SetFromChannelValue(tunable, &(param->val));
        }
    }

    return retVal;
}


McxStatus Fmu2SetVariable(Fmu2CommonStruct * fmu, Fmu2Value * fmuVal) {
    fmi2_status_t status = fmi2_status_ok;

    char * const name = fmuVal->name;

    Channel * channel = fmuVal->channel;
    if (channel && FALSE == channel->IsDefinedDuringInit(channel)) {
        MCX_DEBUG_LOG("Fmu2SetVariable: %s not set: no new value", fmuVal->name);
        return RETURN_OK;
    }

    ChannelValue * const chVal = &fmuVal->val;
    ChannelType type = ChannelValueType(chVal);

    switch (type) {
    case CHANNEL_DOUBLE:
    {
        fmi2_value_reference_t vr[] = {fmuVal->data->vr.scalar};

        status = fmi2_import_set_real(fmu->fmiImport, vr, 1, (const fmi2_real_t *) ChannelValueReference(chVal));

        MCX_DEBUG_LOG("Set %s(%d)=%f", fmuVal->name, vr[0], *(double*)ChannelValueReference(chVal));

        break;
    }
    case CHANNEL_INTEGER:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_set_integer(fmu->fmiImport, vr, 1, (const fmi2_integer_t *) ChannelValueReference(chVal));

        MCX_DEBUG_LOG("Set %s(%d)=%d", fmuVal->name, vr[0], *(int*)ChannelValueReference(chVal));

        break;
    }
    case CHANNEL_BOOL:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_set_boolean(fmu->fmiImport, vr, 1, (const fmi2_boolean_t *) ChannelValueReference(chVal));

        break;
    }
    case CHANNEL_STRING:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_set_string(fmu->fmiImport, vr, 1, (fmi2_string_t *) ChannelValueReference(chVal));

        break;
    }
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
    {
        binary_string * binary = (binary_string *) ChannelValueReference(chVal);

        fmi2_value_reference_t vrs [] = { fmuVal->data->vr.binary.lo
                                        , fmuVal->data->vr.binary.hi
                                        , fmuVal->data->vr.binary.size
                                        };

        fmi2_integer_t vals[] = { (fmi2_integer_t)  ((long long)binary->data & (long long)0x00000000ffffffff)
                                , (fmi2_integer_t) (((long long)binary->data & (long long)0xffffffff00000000) >> 32)
                                , (fmi2_integer_t) binary->len
                                };

        status = fmi2_import_set_integer(fmu->fmiImport, vrs, 3, vals);

        break;
    }
    default:
        mcx_log(LOG_ERROR, "FMU: Unknown variable type");
        return RETURN_ERROR;
    }

    if (fmi2_status_ok != status) {
        if (fmi2_status_error == status || fmi2_status_fatal == status) {
            fmu->runOk = fmi2_false;
            mcx_log(LOG_ERROR, "FMU: Setting of variable %s failed", name);
            return RETURN_ERROR;
        } else {
            if (fmi2_status_warning == status) {
                mcx_log(LOG_WARNING, "FMU: Setting of variable %s return with a warning", name);
            } else if (fmi2_status_discard == status) {
                mcx_log(LOG_WARNING, "FMU: Setting of variable %s discarded", name);
            }
        }
    }

    return RETURN_OK;
}

McxStatus Fmu2SetVariableArray(Fmu2CommonStruct * fmu, ObjectContainer * vals) {
    size_t i = 0;
    size_t numVars = vals->Size(vals);

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < numVars; i++) {
        Fmu2Value * const fmuVal = (Fmu2Value *) vals->At(vals, i);

        retVal = Fmu2SetVariable(fmu, fmuVal);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus Fmu2GetVariable(Fmu2CommonStruct * fmu, Fmu2Value * fmuVal) {
    fmi2_status_t status = fmi2_status_ok;

    char * const name = fmuVal->name;
    ChannelValue * const chVal = &fmuVal->val;

    ChannelType type = ChannelValueType(chVal);

    switch (type) {
    case CHANNEL_DOUBLE:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_get_real(fmu->fmiImport, vr, 1, (fmi2_real_t *) ChannelValueReference(chVal));

        MCX_DEBUG_LOG("Get %s(%d)=%f", fmuVal->name, vr[0], *(double*)ChannelValueReference(chVal));

        break;
    }
    case CHANNEL_INTEGER:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_get_integer(fmu->fmiImport, vr, 1, (fmi2_integer_t *) ChannelValueReference(chVal));

        break;
    }
    case CHANNEL_BOOL:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_get_boolean(fmu->fmiImport, vr, 1, (fmi2_boolean_t *) ChannelValueReference(chVal));

        break;
    }
    case CHANNEL_STRING:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        char * buffer = NULL;

        status = fmi2_import_get_string(fmu->fmiImport, vr, 1, (fmi2_string_t *) &buffer);
        ChannelValueSetFromReference(chVal, &buffer);

        break;
    }
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
    {
        fmi2_value_reference_t vrs [] = { fmuVal->data->vr.binary.lo
                                        , fmuVal->data->vr.binary.hi
                                        , fmuVal->data->vr.binary.size
                                        };

        fmi2_integer_t vs[] = {0, 0, 0};

        binary_string binary;

        status = fmi2_import_get_integer(fmu->fmiImport, vrs, 3, vs);

    binary.len = vs[2];
    binary.data = (char *) ((((long long)vs[1] & 0xffffffff) << 32) | (vs[0] & 0xffffffff));

        ChannelValueSetFromReference(chVal, &binary);

        break;
    }
    default:
        mcx_log(LOG_WARNING, "FMU: Unknown variable type of variable %s", name);
        return RETURN_ERROR;
    }

    if (fmi2_status_ok != status) {
        if (fmi2_status_error == status || fmi2_status_fatal == status) {
            fmu->runOk = fmi2_false;
            mcx_log(LOG_ERROR, "FMU: Getting of variable %s failed", name);
            return RETURN_ERROR;
        } else {
            if (fmi2_status_warning == status) {
                mcx_log(LOG_WARNING, "FMU: Getting of variable %s return with a warning", name);
            } else if (fmi2_status_discard == status) {
                mcx_log(LOG_WARNING, "FMU: Getting of variable %s discarded", name);
            }
        }
    }

    return RETURN_OK;
}

McxStatus Fmu2GetVariableArray(Fmu2CommonStruct * fmu, ObjectContainer * vals) {
    size_t i = 0;
    size_t numVars = vals->Size(vals);

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < numVars; i++) {
        Fmu2Value * const fmuVal = (Fmu2Value *) vals->At(vals, i);

        retVal = Fmu2GetVariable(fmu, fmuVal);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "FMU: Getting of variable array failed at element %u", i);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

int fmi2FilterLocalVariables(fmi2_import_variable_t *vl, void *data) {
    fmi2_causality_enu_t causality = fmi2_import_get_causality(vl);
    if (fmi2_causality_enu_local == causality) {
        return 1;
    } else {
        return 0;
    }
}

int fmi2FilterTunableParameters(fmi2_import_variable_t *vl, void *data) {
    fmi2_causality_enu_t causality = fmi2_import_get_causality(vl);
    if (fmi2_causality_enu_parameter == causality) {
        fmi2_variability_enu_t variability = fmi2_import_get_variability(vl);
        if (fmi2_variability_enu_tunable == variability) {
            return 1;
        }
    }
    return 0;
}

McxStatus Fmi2RegisterLocalChannelsAtDatabus(ObjectContainer * vals, const char * compName, Databus * db) {
    size_t sizeList, i;
    char * buffer = NULL;
    McxStatus retVal = RETURN_OK;

    sizeList = vals->Size(vals);

    for (i = 0; i < sizeList; i++) {
        Fmu2Value * val = (Fmu2Value *) vals->At(vals, i);
        const char * name = val->name;
        fmi2_import_unit_t * unit = NULL;
        const char * unitName;
        ChannelType type = ChannelValueType(&val->val);

        if (CHANNEL_DOUBLE == type) {
            unit = fmi2_import_get_real_variable_unit(fmi2_import_get_variable_as_real(val->data->data.scalar));
        }
        if (unit) {
            unitName = fmi2_import_get_unit_name(unit);
        } else {
            unitName = DEFAULT_NO_UNIT;
        }

        buffer = CreateChannelID(compName, name);
        if (!buffer) {
            mcx_log(LOG_ERROR, "%s: Creation of channel ID for channel %s failed", compName, name);
            return RETURN_ERROR;
        }

        retVal = DatabusAddLocalChannel(db, name, buffer, unitName, ChannelValueReference(&val->val), ChannelValueType(&val->val));
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "%s: Adding channel %s to databus failed", compName, name);
            return RETURN_ERROR;
        }

        mcx_free(buffer);
    }
    return RETURN_OK;
}

ObjectContainer * Fmu2ValueScalarListFromVarList(fmi2_import_variable_list_t * vars) {
    ObjectContainer * list = (ObjectContainer *) object_create(ObjectContainer);

    size_t i, num;

    if (!list) {
        return NULL;
    }

    num = fmi2_import_get_variable_list_size(vars);
    for (i = 0; i < num; i++) {
        fmi2_import_variable_t * var = fmi2_import_get_variable(vars, i);
        char * name = (char *)fmi2_import_get_variable_name(var);
        ChannelType type = Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var));

        Fmu2Value * value = Fmu2ValueScalarMake(name, var, NULL, NULL);
        if (value) {
            list->PushBack(list, (Object *) value);
        } else {
            list->DestroyObjects(list);
            object_destroy(list);

            return NULL;
        }
    }

    return list;
}

ObjectContainer * Fmu2ValueScalarListFromValVarList(ObjectContainer * vals, fmi2_import_variable_list_t * vars) {
    ObjectContainer * list = (ObjectContainer *) object_create(ObjectContainer);

    size_t i, num;

    if (!list) {
        return NULL;
    }

    if (vals->Size(vals) != fmi2_import_get_variable_list_size(vars)) {
        return NULL;
    }

    num = fmi2_import_get_variable_list_size(vars);
    for (i = 0; i < num; i++) {
        fmi2_import_variable_t * var = fmi2_import_get_variable(vars, i);
        char * name = (char *)fmi2_import_get_variable_name(var);
        ChannelType type = Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var));
        ChannelValue * chVal = (ChannelValue *) vals->At(vals, i);
        Fmu2Value * value = NULL;

        if (strcmp(name, vals->GetElementName(vals, i))) {
            return NULL;
        }

        value = Fmu2ValueScalarMake(name, var, NULL, NULL);
        if (value) {
            McxStatus retVal = value->SetFromChannelValue(value, chVal);
            if (RETURN_OK != retVal) {
                object_destroy(value);

                list->DestroyObjects(list);
                object_destroy(list);

                return NULL;
            }

            list->PushBack(list, (Object *) value);
        } else {
            list->DestroyObjects(list);
            object_destroy(list);

            return NULL;
        }
    }

    return list;
}

fmi2_import_variable_list_t * Fmu2GetFilteredVars(fmi2_import_t * import, int (* filter)(fmi2_import_variable_t *vl, void *data)) {
    fmi2_import_variable_list_t * vars = NULL;
    fmi2_import_variable_list_t * tunables = NULL;

    vars = fmi2_import_get_variable_list(import, 0);
    if (!vars) {
        return NULL;
    }

    tunables = fmi2_import_filter_variables(vars, filter, NULL);
    if (!tunables) {
        fmi2_import_free_variable_list(vars);
        return NULL;
    }

    fmi2_import_free_variable_list(vars);

    return tunables;
}

ObjectContainer * Fmu2ReadTunableParams(fmi2_import_t * import) {
    fmi2_import_variable_list_t * tunables = Fmu2GetFilteredVars(import, fmi2FilterTunableParameters);

    ObjectContainer * list = Fmu2ValueScalarListFromVarList(tunables);

    fmi2_import_free_variable_list(tunables);

    return list;
}

ObjectContainer * Fmu2ReadLocalVariables(fmi2_import_t * import) {
    fmi2_import_variable_list_t * local = Fmu2GetFilteredVars(import, fmi2FilterLocalVariables);

    ObjectContainer * list = Fmu2ValueScalarListFromVarList(local);

    fmi2_import_free_variable_list(local);

    return list;
}

McxStatus Fmu2CheckTunableParamsInputConsistency(ObjectContainer * in, ObjectContainer * params,  ObjectContainer * tunableParams) {
        // Ensure tunables-as-inputs are not also used as tunable parameters
        mcx_log(LOG_DEBUG, "Inputs: ");
        in->Iterate(in, (fObjectIter) Fmu2ValuePrintDebug);

        mcx_log(LOG_DEBUG, "Parameters: ");
        params->Iterate(params, (fObjectIter) Fmu2ValuePrintDebug);

        mcx_log(LOG_DEBUG, "Tunable Parameters: ");
        tunableParams->Iterate(tunableParams, (fObjectIter) Fmu2ValuePrintDebug);

        McxStatus retVal = RETURN_OK;

        ObjectContainer * vals_ = params->FilterCtx(params, Fmu2ValueIsContainedInObjectContainerPred, in);
        if (!vals_) {
            retVal = RETURN_ERROR;
            goto cleanup;
        } else if (vals_->Size(vals_) > 0) {
            mcx_log(LOG_ERROR, "Parameters are also defined as inputs");
            retVal = RETURN_ERROR;
            goto cleanup;
        }

cleanup:
        if (vals_) {
            object_destroy(vals_);
        }

        return retVal;
}

void Fmu2MarkTunableParamsAsInputAsDiscrete(ObjectContainer * in) {
    size_t i = 0;

    // Mark tunables as discrete so that they are updated correctly
    for (i = 0; i < in->Size(in); i++) {
        Fmu2Value * val = (Fmu2Value *) in->At(in, i);

        if (val->data->type == FMU2_VALUE_SCALAR) {
            fmi2_import_variable_t * var = val->data->data.scalar;
            fmi2_causality_enu_t causality = fmi2_import_get_causality(var);

            if (fmi2_causality_enu_input != causality) {
                ChannelIn * in = (ChannelIn *) val->channel;
                ChannelInfo * info = &((Channel*)in)->info;
                mcx_log(LOG_DEBUG, "Setting input \"%s\" as discrete", ChannelInfoGetLogName(info));
                in->SetDiscrete(in);
            }
        }
    }
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */