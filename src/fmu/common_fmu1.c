/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "fmu/common_fmu1.h"
#include "fmu/common_fmu.h"

#include "fmu/Fmu1Value.h"
#include "reader/model/parameters/ParameterInput.h"

#include "util/string.h"
#include "util/signals.h"

#include "fmilib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
* converts a fmiStatus variable to a string. only used for logging
* @param status to translate
* @return the string representation of the given status
*/
static const char * FmiStatus2String(fmi1_status_t status) {
    switch (status) {
    case fmi1_status_ok:
        return "OK";
    case fmi1_status_warning:
        return "WARNING";
    case fmi1_status_discard:
        return "DISCARD";
    case fmi1_status_error:
        return "ERROR";
    case fmi1_status_fatal:
        return "FATAL";
    default:
        return "???";
    }
}

/**
 * a callback function for the fmu-dll to write logging messages
 * @param comp fmi component
 * @param obj pointer to the memory that should be freed
 * @param obj pointer to the memory that should be freed
 */
static void FmiLogger(fmi1_component_t c, fmi1_string_t instanceName, fmi1_status_t status, fmi1_string_t category, fmi1_string_t message, ...) {
#define MAX_MSG_SIZE       2000
    char msg[MAX_MSG_SIZE];

    va_list argp;

    va_start(argp, message);
    vsprintf(msg, message, argp);

    if (!instanceName)
        instanceName = "?";
    if (!category)
        category = "?";

    if (fmi1_status_ok == status) {
        mcx_log(LOG_DEBUG, "%s: [%s][FMU status:%s]: %s", instanceName, category, FmiStatus2String(status), msg);
    } else {
        mcx_log(LOG_INFO, "%s: [%s][FMU status:%s]: %s", instanceName, category, FmiStatus2String(status), msg);
    }
}


/**
 * a callback function for the FMU interface to allocate memory
 * @param nobj number of objects to allocate
 * @param size of one object
 * @return a pointer to the allocated memory
 */
static void * FmiAllocateMemory(size_t nobj, size_t size) {
    void * ptr = NULL;

    ptr = mcx_calloc(nobj, size);

    if (NULL == ptr && nobj > 0 && size > 0) {
        mcx_log(LOG_ERROR, "FMU: NULL pointer allocation: %zu objects of size %d", nobj, size);
    }

    return ptr;
}


/**
 * a callback function for the fmu-dll to free memory
 * @param obj pointer to the memory that should be freed
 */
static void FmiFreeMemory(void * obj) {
    if (NULL == obj) {
        mcx_log(LOG_ERROR, "FMU: NULL pointer free");
    } else {
        mcx_free(obj);
    }
}

static fmi1_callback_functions_t fmi1Callbacks = {
    FmiLogger,
    FmiAllocateMemory,
    FmiFreeMemory,
    NULL
};

ChannelType * Fmi1TypeToChannelType(fmi1_base_type_enu_t type) {
    switch (type) {
    case fmi1_base_type_real:
        return &ChannelTypeDouble;
    case fmi1_base_type_int:
        return &ChannelTypeInteger;
    case fmi1_base_type_bool:
        return &ChannelTypeBool;
    case fmi1_base_type_str:
        return &ChannelTypeString;
    case fmi1_base_type_enum:
        return &ChannelTypeInteger;
    default:
        return &ChannelTypeUnknown;
    }
}

McxStatus Fmu1CommonStructInit(Fmu1CommonStruct * fmu) {
    fmu->fmiImport = NULL;

    fmu->instantiateOk = fmi1_false;
    fmu->runOk = fmi1_false;
    fmu->isLogging = fmi1_false;

    fmu->in = (ObjectContainer *) object_create(ObjectContainer);
    fmu->out = (ObjectContainer *) object_create(ObjectContainer);
    fmu->params = (ObjectContainer *) object_create(ObjectContainer);
    fmu->initialValues = (ObjectContainer *) object_create(ObjectContainer);

    fmu->localValues = (ObjectContainer *) object_create(ObjectContainer);

    return RETURN_OK;
}

McxStatus Fmu1CommonStructRead(FmuCommon * common, Fmu1CommonStruct * fmu1, Fmu1Type fmu_type, FmuInput * fmuInput) {
    jm_status_enu_t jmStatus = jm_status_success;
    fmi1_fmu_kind_enu_t fmu1_kind = fmi1_fmu_kind_enu_unknown;

    int logging = 0;
    McxStatus retVal = RETURN_OK;

    fmu1->isLogging = fmuInput->isLogging.defined && fmuInput->isLogging.value ? fmi1_true : fmi1_false;

    if (!common->instanceName) {
        mcx_log(LOG_ERROR, "FMU instance does not have a name");
        return RETURN_ERROR;
    }

    if (common->version != fmi_version_1_enu) {
        mcx_log(LOG_ERROR, "%s: FMU Version mismatch", common->instanceName);
        return RETURN_ERROR;
    }

    fmu1->fmiImport = fmi1_import_parse_xml(common->context, common->path);
    if (NULL == fmu1->fmiImport) {
        mcx_log(LOG_ERROR, "%s: FMI 1.0 XML file could not be parsed", common->instanceName);
        return RETURN_ERROR;
    }


    fmu1_kind = fmi1_import_get_fmu_kind(fmu1->fmiImport);

    if (fmu_type == Fmu1CoSimulation) {
        if (fmi1_fmu_kind_enu_cs_standalone == fmu1_kind) {
            mcx_log(LOG_DEBUG, "%s: FMU co-simulation interface (standalone) loaded", common->instanceName);
        } else if (fmi1_fmu_kind_enu_cs_tool == fmu1_kind) {
            mcx_log(LOG_DEBUG, "%s: FMU co-simulation interface (tool) loaded", common->instanceName);
        } else {
            mcx_log(LOG_ERROR, "%s: FMU does not contain a co-simulation interface", common->instanceName);
            return RETURN_ERROR;
        }
    }
    else {
        mcx_log(LOG_ERROR, "%s: unknown kind of fmu", common->instanceName);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus Fmu1CommonStructSetup(FmuCommon * common, Fmu1CommonStruct * fmu1, Fmu1Type fmu_type) {
    jm_status_enu_t jmStatus = jm_status_success;
    fmi1_status_t status;
    McxStatus retVal = RETURN_OK;

    jmStatus = fmi1_import_create_dllfmu(fmu1->fmiImport,
                                         fmi1Callbacks,
                                         0 /* register globally */
                                         );
    if (jm_status_success != jmStatus) {
        mcx_log(LOG_ERROR, "%s: Could not load FMU dll", common->instanceName);
        fmu1->fmiImport = NULL;
        return RETURN_ERROR;
    }


    if (fmu_type == Fmu1CoSimulation) {
        jmStatus = fmi1_import_instantiate_slave(fmu1->fmiImport,
                                                 common->instanceName,
                                                 NULL, /* resource dir is gotten from extracted fmu */
                                                 NULL, /* mimeType: application/x-fmu-sharedlibrary */
                                                 0.0, /* timeout */
                                                 fmi1_false, /* visible */
                                                 fmi1_false /* interactive */
                                                 );
        if (jm_status_success != jmStatus) {
            mcx_log(LOG_ERROR, "%s: Instantiate failed", common->instanceName);
            return RETURN_ERROR;
        }
    }
    else {
        mcx_log(LOG_ERROR, "%s: unknown kind of fmu", common->instanceName);
        return RETURN_ERROR;
    }
    fmu1->instantiateOk = fmi1_true;

    status = fmi1_import_set_debug_logging(fmu1->fmiImport, fmu1->isLogging);
    if (fmi1_status_ok != status) {
        mcx_log(LOG_ERROR, "%s: Setting debug mode failed", common->instanceName);
        if (fmi1_status_error == status || fmi1_status_fatal == status) {
            fmu1->runOk = fmi1_false;
        }
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

void Fmu1CommonStructDestructor(Fmu1CommonStruct * fmu) {
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

    if (fmu->fmiImport) {
        fmi1_import_free(fmu->fmiImport);
        fmu->fmiImport = NULL;
    }
}

Fmu1Value * Fmu1ReadParamValue(ScalarParameterInput * input, fmi1_import_t * import) {
    ChannelValue chVal;
    Fmu1Value * val = NULL;
    fmi1_import_variable_t * var = NULL;
    McxStatus retVal = RETURN_OK;

    if (!input->value.defined) {
        mcx_log(LOG_ERROR, "Parameter %s: No value defined", input->name);
        return NULL;
    }

    ChannelValueInit(&chVal, ChannelTypeClone(input->type));
    if (RETURN_OK != ChannelValueSetFromReference(&chVal, &input->value.value)) {
        return NULL;
    }

    var = fmi1_import_get_variable_by_name(import, input->name);
    if (!var) {
        return NULL;
    }

    val = Fmu1ValueScalarMake(input->name, var, NULL);
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

static ObjectContainer * Fmu1ReadArrayParamValues(const char * name,
                                                  ArrayParameterInput * input,
                                                  fmi1_import_t * import,
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
        goto cleanup_0;
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
            Fmu1Value * val = NULL;
            char * varName = (char *)mcx_calloc(stringBufferLength, sizeof(char));
            fmi1_import_variable_t * var = NULL;
            ChannelValue chVal;

            if (!varName) {
                retVal = RETURN_ERROR;
                goto cleanup_1;
            }

            if (input->numDims == 2) {
                snprintf(varName, stringBufferLength, "%s[%zu,%zu]", name, k, j);
            }
            else {
                snprintf(varName, stringBufferLength, "%s[%zu]", name, j);
            }

            // check if the parameter was already defined
            if (params && params->GetByName(params, varName) != NULL) {
                mcx_log(LOG_DEBUG, "FMU: Duplicate definition of parameter %s", varName);
                retVal = RETURN_ERROR;
                goto cleanup_1;
            }

            var = fmi1_import_get_variable_by_name(import, varName);
            if (!var) {
                mcx_log(LOG_ERROR, "FMU: Could not get variable %s", varName);
                retVal = RETURN_ERROR;
                goto cleanup_1;
            }

            val = Fmu1ValueScalarMake(varName, var, NULL);
            if (!val) {
                retVal = RETURN_ERROR;
                goto cleanup_1;
            }

            if (ChannelTypeEq(input->type, &ChannelTypeDouble)) {
                ChannelValueInit(&chVal, ChannelTypeClone(&ChannelTypeDouble));
                if (RETURN_OK != ChannelValueSetFromReference(&chVal, &((double *)input->values)[index])) {
                    return RETURN_ERROR;
                }
            }
            else { // integer
                ChannelValueInit(&chVal, ChannelTypeClone(&ChannelTypeInteger));
                if (RETURN_OK != ChannelValueSetFromReference(&chVal, &((int *)input->values)[index])) {
                    return RETURN_ERROR;
                }
            }

            retVal = val->SetFromChannelValue(val, &chVal);
            if (RETURN_OK != retVal) {
                retVal = RETURN_ERROR;
                goto cleanup_1;
            }

            // store value
            retVal = values->PushBack(values, (Object *)val);
            if (RETURN_OK != retVal) {
                retVal = RETURN_ERROR;
                goto cleanup_1;
            }

        cleanup_1:
            if (varName) { mcx_free(varName); }

            if (retVal == RETURN_ERROR) {
                if (val) { object_destroy(val); }
                goto cleanup_0;
            }
        }
    }

cleanup_0:
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
// Ignores parameters that are provided via the `ignore` argument.
ObjectContainer * Fmu1ReadParams(ParametersInput * input, fmi1_import_t * import, ObjectContainer * ignore) {
    ObjectContainer * params = (ObjectContainer *)object_create(ObjectContainer);
    ObjectContainer * ret = params;                  // used for unified cleanup via goto

    size_t i = 0;
    size_t num = 0;
    McxStatus retVal = RETURN_OK;

    if (!params) {
        return NULL;
    }

    num = input->parameters->Size(input->parameters);
    for (i = 0; i < num; i++) {
        ParameterInput * parameterInput = (ParameterInput *)input->parameters->At(input->parameters, i);

        char * name = NULL;
        Fmu1Value * val = NULL;
        ObjectContainer * vals = NULL;

        name = mcx_string_copy(parameterInput->parameter.arrayParameter->name);
        if (!name) {
            ret = NULL;
            goto cleanup;
        }

        // check whether a parameter with the same name was already defined
        if (params->GetByName(params, name) != NULL) {
            mcx_log(LOG_ERROR, "FMU: Duplicate definition of parameter %s", name);
            ret = NULL;
            goto cleanup;
        }

        // ignore the parameter if it is in the `ignore` container
        if (ignore && ignore->GetNameIndex(ignore, name) >= 0) {
            goto cleanup;
        }

        if (parameterInput->type == PARAMETER_ARRAY) {
            // read parameter dimensions (if any) - should only be defined for array parameters
            if (parameterInput->parameter.arrayParameter->numDims >= 2 &&
                parameterInput->parameter.arrayParameter->dims[1] &&
                !parameterInput->parameter.arrayParameter->dims[0]) {
                mcx_log(LOG_ERROR, "FMU: Array parameter %s: Missing definition for the first dimension "
                        "while the second dimension is defined.", parameterInput->parameter.arrayParameter->name);
                ret = NULL;
                goto cleanup;
            }

            // array - split it into scalars
            vals = Fmu1ReadArrayParamValues(name, parameterInput->parameter.arrayParameter, import, params);
            if (vals == NULL) {
                ret = NULL;
                goto cleanup;
            }

            // store the scalar values
            size_t j = 0;
            for (j = 0; j < vals->Size(vals); j++) {
                Object * v = vals->At(vals, j);
                retVal = params->PushBackNamed(params, v, ((Fmu1Value *)v)->name);
                if (RETURN_OK != retVal) {
                    ret = NULL;
                    goto cleanup;
                }
            }

            object_destroy(vals);
        } else {
            // read the scalar value
            val = Fmu1ReadParamValue(parameterInput->parameter.scalarParameter, import);
            if (val == NULL) {
                ret = NULL;
                goto cleanup;
            }

            // store the read value
            retVal = params->PushBackNamed(params, (Object *)val, name);
            if (RETURN_OK != retVal) {
                ret = NULL;
                goto cleanup;
            }
        }

cleanup:
        if (name) { mcx_free(name); }
        if (ret == NULL) {
            if (val) { object_destroy(val); }
            if (vals) {
                vals->DestroyObjects(vals);
                object_destroy(vals);
            }
            params->DestroyObjects(params);
            object_destroy(params);
            break;
        }
    }

    return ret;
}

McxStatus Fmu1SetVariable(Fmu1CommonStruct * fmu, Fmu1Value * fmuVal) {
    fmi1_status_t status = fmi1_status_ok;

    Channel * channel = fmuVal->channel;
    if (channel && FALSE == channel->IsDefinedDuringInit(channel)) {
        MCX_DEBUG_LOG("Fmu1SetVariable: %s not set: no defined value during initialization", fmuVal->name);
        return RETURN_OK;
    }

    ChannelValue * const chVal = &fmuVal->val;
    ChannelType * type = ChannelValueType(chVal);

    switch (type->con) {
    case CHANNEL_DOUBLE:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        double value = chVal->value.d;
        if (fmi1_variable_is_negated_alias == fmi1_import_get_variable_alias_kind(fmuVal->data->var.scalar)) {
            value *= -1.;
        }
        status = fmi1_import_set_real(fmu->fmiImport, vr, 1, &value);
    }
    break;
    case CHANNEL_INTEGER:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        int value = chVal->value.i;
            if (fmi1_variable_is_negated_alias == fmi1_import_get_variable_alias_kind(fmuVal->data->var.scalar)) {
            value *= -1;
        }
        status = fmi1_import_set_integer(fmu->fmiImport, vr, 1, &value);
    }
    break;
    case CHANNEL_BOOL:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        fmi1_boolean_t value = chVal->value.i;
        if (fmi1_variable_is_negated_alias == fmi1_import_get_variable_alias_kind(fmuVal->data->var.scalar)) {
            value = !value;
        }
        status = fmi1_import_set_boolean(fmu->fmiImport, vr, 1, &value);
    }
    break;
    case CHANNEL_STRING:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        status = fmi1_import_set_string(fmu->fmiImport, vr, 1, (fmi1_string_t *) &chVal->value.s);
        break;
    }
    case CHANNEL_ARRAY:
    {
        fmi1_value_reference_t * vrs = fmuVal->data->vr.array.values;
        mcx_array * a = (mcx_array *) ChannelValueReference(&fmuVal->val);

        size_t num = mcx_array_num_elements(a);
        void * vals = a->data;

        if (ChannelTypeEq(a->type, &ChannelTypeDouble)) {
            status = fmi1_import_set_real(fmu->fmiImport, vrs, num, vals);
        } else if (ChannelTypeEq(a->type, &ChannelTypeInteger)) {
            status = fmi1_import_set_integer(fmu->fmiImport, vrs, num, vals);
        } else {
            mcx_log(LOG_ERROR, "FMU: Unsupported array variable type: %s", ChannelTypeToString(a->type));
            return RETURN_ERROR;
        }
        break;
    }
    default:
        mcx_log(LOG_WARNING, "FMU: Unknown variable type");
        break;
    }

    if (fmi1_status_ok != status) {
        if (fmi1_status_error == status || fmi1_status_fatal == status) {
            fmu->runOk = fmi1_false;
            mcx_log(LOG_ERROR, "FMU: Setting of variable %s failed", fmuVal->name);
            return RETURN_ERROR;
        } else {
            if (fmi1_status_warning == status) {
                mcx_log(LOG_WARNING, "FMU: Setting of variable %s returned with a warning", fmuVal->name);
            } else if (fmi1_status_discard == status) {
                mcx_log(LOG_WARNING, "FMU: Setting of variable %s discarded", fmuVal->name);
            }
        }
    } else {
    }
    return RETURN_OK;
}


McxStatus Fmu1SetVariableArray(Fmu1CommonStruct * fmu, ObjectContainer * vals) {
    size_t i = 0;
    size_t numVars = vals->Size(vals);

    mcx_signal_handler_set_this_function();

    for (i = 0; i < numVars; i++) {
        Fmu1Value * fmuVal = (Fmu1Value *) vals->At(vals, i);

        if (RETURN_ERROR == Fmu1SetVariable(fmu, fmuVal)) {
            mcx_signal_handler_unset_function();
            return RETURN_ERROR;
        }
    }

    mcx_signal_handler_unset_function();

    return RETURN_OK;
}


McxStatus Fmu1GetVariable(Fmu1CommonStruct * fmu, Fmu1Value * fmuVal) {
    fmi1_status_t status = fmi1_status_ok;

    ChannelValue * const chVal = &fmuVal->val;
    ChannelType * type = ChannelValueType(chVal);

    switch (type->con) {
    case CHANNEL_DOUBLE:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        status = fmi1_import_get_real(fmu->fmiImport, vr, 1, (fmi1_real_t *) ChannelValueReference(chVal));
        if (fmi1_variable_is_negated_alias == fmi1_import_get_variable_alias_kind(fmuVal->data->var.scalar)) {
            fmuVal->val.value.d *= -1.;
        }
        break;
    }
    case CHANNEL_INTEGER:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        status = fmi1_import_get_integer(fmu->fmiImport, vr, 1, (fmi1_integer_t *) ChannelValueReference(chVal));
        if (fmi1_variable_is_negated_alias == fmi1_import_get_variable_alias_kind(fmuVal->data->var.scalar)) {
            fmuVal->val.value.i *= -1;
        }
        break;
    }
    case CHANNEL_BOOL:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        status = fmi1_import_get_boolean(fmu->fmiImport, vr, 1, (fmi1_boolean_t *) ChannelValueReference(chVal));
        if (fmi1_variable_is_negated_alias == fmi1_import_get_variable_alias_kind(fmuVal->data->var.scalar)) {
            fmuVal->val.value.i = !fmuVal->val.value.i;
        }
        break;
    }
    case CHANNEL_STRING:
    {
        fmi1_value_reference_t vr[] = {fmuVal->data->vr.scalar};
        char * buffer = NULL;

        status = fmi1_import_get_string(fmu->fmiImport, vr, 1, (fmi1_string_t *)&buffer);
        if (RETURN_OK != ChannelValueSetFromReference(chVal, &buffer)) {
            return RETURN_ERROR;
        }
        break;
    }
    case CHANNEL_ARRAY:
    {
        fmi1_value_reference_t * vrs = fmuVal->data->vr.array.values;
        mcx_array * a = (mcx_array *) ChannelValueReference(&fmuVal->val);

        size_t num = mcx_array_num_elements(a);
        void * vals = a->data;

        if (ChannelTypeEq(a->type, &ChannelTypeDouble)) {
            status = fmi1_import_get_real(fmu->fmiImport, vrs, num, vals);
        } else if (ChannelTypeEq(a->type, &ChannelTypeInteger)) {
            status = fmi1_import_get_integer(fmu->fmiImport, vrs, num, vals);
        } else {
            mcx_log(LOG_ERROR, "FMU: Unsupported array variable type: %s", ChannelTypeToString(a->type));
            return RETURN_ERROR;
        }
        break;
    }
    default:
        mcx_log(LOG_WARNING, "FMU: Unknown variable type");
        break;
    }

    if (fmi1_status_ok != status) {
        if (fmi1_status_error == status || fmi1_status_fatal == status) {
            fmu->runOk = fmi1_false;
            mcx_log(LOG_ERROR, "FMU: Getting of variable %s failed", fmuVal->name);
            return RETURN_ERROR;
        } else {
            // TODO: handle warning
        }
    }

    return RETURN_OK;
}


McxStatus Fmu1GetVariableArray(Fmu1CommonStruct * fmu, ObjectContainer * vals) {
    size_t i = 0;
    size_t numVars = vals->Size(vals);

    mcx_signal_handler_set_this_function();

    for (i = 0; i < numVars; i++) {
        McxStatus retVal = RETURN_OK;
        Fmu1Value * fmuVal = (Fmu1Value *) vals->At(vals, i);
        retVal = Fmu1GetVariable(fmu, fmuVal);
        if (RETURN_ERROR == retVal) {
            mcx_signal_handler_unset_function();
            return RETURN_ERROR;
        }
    }

    mcx_signal_handler_unset_function();

    return RETURN_OK;
}

int fmi1FilterLocalVariables(fmi1_import_variable_t *vl, void *data) {
    fmi1_causality_enu_t causality = fmi1_import_get_causality(vl);
    if (fmi1_causality_enu_internal == causality) {
        return 1;
    } else {
        return 0;
    }
}

McxStatus fmi1CreateValuesOutOfVariables(ObjectContainer * vals, fmi1_import_variable_list_t * vars) {
    size_t sizeList;
    unsigned int i;
    McxStatus retVal;
    sizeList = fmi1_import_get_variable_list_size(vars);

    for (i = 0; i < sizeList; i++) {
        fmi1_import_variable_t * actualVar = fmi1_import_get_variable(vars, i);
        Fmu1Value * val = NULL;
        const char * name = fmi1_import_get_variable_name(actualVar);

        val = Fmu1ValueScalarMake(name, actualVar, NULL);

        retVal = vals->PushBack(vals, (Object *)val);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "FMU: could not create value for variable %s", name);
            return RETURN_ERROR;
        }

    }

    return RETURN_OK;
}

McxStatus fmi1AddLocalChannelsFromLocalValues(ObjectContainer * vals, const char * compName, Databus * db) {
    size_t sizeList, i;
    char * buffer = NULL;
    McxStatus retVal = RETURN_OK;
    size_t len = 0;
    sizeList = vals->Size(vals);

    for (i = 0; i < sizeList; i++) {
        Fmu1Value * val = (Fmu1Value *) vals->At(vals, i);
        fmi1_import_variable_t * actualVar = val->data->var.scalar;  // TODO array

        const char * name = NULL;
        fmi1_base_type_enu_t type;
        fmi1_import_unit_t * unit = NULL;
        const char * unitName = NULL;
        name = fmi1_import_get_variable_name(actualVar);
        type = fmi1_import_get_variable_base_type(actualVar);

        if (fmi1_base_type_real == type) {
            unit = fmi1_import_get_real_variable_unit(fmi1_import_get_variable_as_real(actualVar));
        }
        if (unit) {
            unitName = fmi1_import_get_unit_name(unit);
        } else {
            unitName = DEFAULT_NO_UNIT;
        }

        buffer = CreateChannelID(compName, name);
        if (!buffer) {
            mcx_log(LOG_ERROR, "FMU: could not create port ID for port %s", name);
            return RETURN_ERROR;
        }


        retVal = DatabusAddLocalChannel(db, name, buffer, unitName, ChannelValueReference(&val->val), ChannelValueType(&val->val));
        mcx_free(buffer);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "%s: Adding channel %s to databus failed", compName, name);
            return RETURN_ERROR;
        }

    }
    return RETURN_OK;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */