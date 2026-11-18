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
#include "core/channels/ChannelValue.h"
#include "fmu/common_fmu.h" /* for jm callbacks */

#include "fmu/Fmu2Value.h"

#include "core/channels/ChannelInfo.h"
#include "core/parameters/ParameterProxies.h"

#include "reader/model/parameters/ArrayParameterDimensionInput.h"
#include "reader/model/parameters/ParameterInput.h"
#include "reader/model/parameters/ParametersInput.h"

#include "util/string.h"
#include "util/stdlib.h"
#include "util/signals.h"

#include "objects/Map.h"

#include "fmilib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


fmi2_base_type_enu_t ChannelTypeToFmi2Type(ChannelType * type) {
    switch (type->con) {
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

ChannelType * Fmi2TypeToChannelType(fmi2_base_type_enu_t type) {
    switch (type) {
    case fmi2_base_type_real:
        return &ChannelTypeDouble;
    case fmi2_base_type_int:
        return &ChannelTypeInteger;
    case fmi2_base_type_bool:
        return &ChannelTypeBool;
    case fmi2_base_type_str:
        return &ChannelTypeString;
    case fmi2_base_type_enum:
        return &ChannelTypeInteger;
    default:
        return &ChannelTypeUnknown;
    }
}

const char * Fmi2TypeToString(fmi2_base_type_enu_t type) {
    switch (type) {
    case fmi2_base_type_real:
        return "fmi2Real";
    case fmi2_base_type_int:
        return "fmi2Integer";
    case fmi2_base_type_bool:
        return "fmi2Bool";
    case fmi2_base_type_str:
        return "fmi2String";
    case fmi2_base_type_enum:
        return "fmi2Enum";
    }
    return "fmi2Unknown";
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

    fmu->arrayParams = (ObjectContainer *) object_create(ObjectContainer);

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
    mcx_signal_handler_set_function("fmi2_import_instantiate"),
    jmStatus = fmi2_import_instantiate(fmu2->fmiImport,
                                       common->instanceName,
                                       fmu_type,
                                       NULL,
                                       fmi2_false /* visible */);
    mcx_signal_handler_unset_function();
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

    if (fmu->arrayParams) {
        fmu->arrayParams->DestroyObjects(fmu->arrayParams);
        object_destroy(fmu->arrayParams);
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

    ChannelValueInit(&chVal, ChannelTypeClone(input->type));
    if (RETURN_OK != ChannelValueSetFromReference(&chVal, &input->value.value)) {
        return NULL;
    }

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

    for (k = start1; k <= end1; k++) {
        for (j = start2; j <= end2; j++, index++) {
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
                snprintf(varName, stringBufferLength, "%s[%zu]", name, k);
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

            if (ChannelTypeEq(input->type, &ChannelTypeDouble)) {
                ChannelValueInit(&chVal, ChannelTypeClone(&ChannelTypeDouble));
                if (RETURN_OK != ChannelValueSetFromReference(&chVal, &((double *)input->values)[index])) {
                    retVal = RETURN_ERROR;
                    goto fmu2_read_array_param_values_for_cleanup;
                }
            } else { // integer
                ChannelValueInit(&chVal, ChannelTypeClone(&ChannelTypeInteger));
                if (RETURN_OK != ChannelValueSetFromReference(&chVal, &((int *)input->values)[index])) {
                    retVal = RETURN_ERROR;
                    goto fmu2_read_array_param_values_for_cleanup;
                }
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
// If arrayParams is given, creates proxy views to array elements.
// Ignores parameters provided via the `ignore` argument.
McxStatus Fmu2ReadParams(ObjectContainer * params, ObjectContainer * arrayParams, ParametersInput * input, fmi2_import_t * import, ObjectContainer * ignore) {
    McxStatus retVal = RETURN_OK;

    size_t i = 0;
    size_t num = 0;

    if (!params) {
        return RETURN_ERROR;
    }

    num = input->parameters->Size(input->parameters);
    for (i = 0; i < num; i++) {
        ParameterInput * parameterInput = (ParameterInput *) input->parameters->At(input->parameters, i);
        char * name = NULL;

        name = mcx_string_copy(parameterInput->parameter.arrayParameter->name);
        if (!name) {
            retVal = RETURN_ERROR;
            goto cleanup_0;
        }

        // ignore the parameter if it is in the `ignore` container
        if (ignore && ignore->GetNameIndex(ignore, name) >= 0) {
            goto cleanup_0;
        }

        if (parameterInput->type == PARAMETER_ARRAY) {
            ObjectContainer * vals = NULL;
            ArrayParameterProxy * proxy = NULL;
            size_t j = 0;

            // array - split it into scalars
            vals = Fmu2ReadArrayParamValues(name, parameterInput->parameter.arrayParameter, import, params);
            if (vals == NULL) {
                mcx_log(LOG_ERROR, "FMU: Could not read array parameter %s", name);
                retVal = RETURN_ERROR;
                goto cleanup_1;
            }

            if (arrayParams) {
                // set up a proxy that will reference the individual scalars
                proxy = (ArrayParameterProxy *) object_create(ArrayParameterProxy);
                if (!proxy) {
                    mcx_log(LOG_ERROR, "FMU: Creating an array proxy failed: No memory");
                    retVal = RETURN_ERROR;
                    goto cleanup_1;
                }

                retVal = proxy->Setup(proxy, name, parameterInput->parameter.arrayParameter->numDims, parameterInput->parameter.arrayParameter->dims);
                if (RETURN_ERROR == retVal) {
                    mcx_log(LOG_ERROR, "FMU Array parameter %s: Array proxy setup failed", name);
                    goto cleanup_1;
                }
            }

            // store the scalar values
            for (j = 0; j < vals->Size(vals); j++) {
                Fmu2Value * v = (Fmu2Value *) vals->At(vals, j);
                retVal = params->PushBackNamed(params, (Object *) v, v->name);
                if (RETURN_OK != retVal) {
                    mcx_log(LOG_ERROR, "FMU: Adding element #%zu of parameter %s failed", j, name);
                    goto cleanup_1;
                }

                vals->SetAt(vals, j, NULL);

                if (arrayParams) {
                    retVal = proxy->AddValue(proxy, v);
                    if (RETURN_ERROR == retVal) {
                        mcx_log(LOG_ERROR, "FMU: Adding proxy to element #%zu of parameter %s failed", j, name);
                        goto cleanup_1;
                    }
                }
            }

            if (arrayParams) {
                retVal = arrayParams->PushBackNamed(arrayParams, (Object *) proxy, proxy->GetName(proxy));
                if (RETURN_ERROR == retVal) {
                    mcx_log(LOG_ERROR, "FMU: Adding proxy for %s failed", name);
                    goto cleanup_1;
                }
            }

cleanup_1:
            if (RETURN_ERROR == retVal) {
                object_destroy(proxy);
                if (vals) {
                    vals->DestroyObjects(vals);
                }
            }

            object_destroy(vals);

            if (RETURN_ERROR == retVal) {
                goto cleanup_0;
            }
        } else {
            Fmu2Value * val = NULL;

            // read the scalar value
            val = Fmu2ReadParamValue(parameterInput->parameter.scalarParameter, import);
            if (val == NULL) {
                mcx_log(LOG_ERROR, "FMU: Could not read parameter value of parameter %s", name);
                retVal = RETURN_ERROR;
                goto cleanup_2;
            }

            // store the read value
            retVal = params->PushBackNamed(params, (Object * ) val, name);
            if (RETURN_OK != retVal) {
                goto cleanup_2;
            }

cleanup_2:
            if (RETURN_ERROR == retVal) {
                object_destroy(val);
            }
        }

cleanup_0:
        if (name) { mcx_free(name); }
        if (retVal == RETURN_ERROR) {
            return RETURN_ERROR;
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
                return RETURN_ERROR;
            }

            for (i = 0; i < n - 1; i++) {
                Fmu2Value * a = (Fmu2Value *) params->At(params, i);
                Fmu2Value * b = (Fmu2Value *) params->At(params, i + 1);

                if (!strcmp(a->name, b->name)) {
                    mcx_log(LOG_ERROR, "FMU: Duplicate definition of parameter %s", a->name);
                    return RETURN_ERROR;
                }
            }
        }
    }

    return retVal;
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

typedef struct ConnectedElems{
    int is_connected;
    size_t num_elems;
    size_t * elems;
} ConnectedElems;

typedef struct ChannelElement {
    size_t channel_idx;
    size_t elem_idx;
} ChannelElement;

static Vector * GetAllElems(ChannelElement * elems, size_t num, size_t channel_idx) {
    size_t i = 0;
    Vector * indices = (Vector *) object_create(Vector);

    if (!indices) {
        mcx_log(LOG_ERROR, "GetAllElems: Not enough memory");
        return NULL;
    }

    indices->Setup(indices, sizeof(size_t), NULL, NULL, NULL);

    for (i = 0; i < num; i++) {
        if (elems[i].channel_idx == channel_idx) {
            if (RETURN_ERROR == indices->PushBack(indices, &elems[i].elem_idx)) {
                mcx_log(LOG_ERROR, "GetAllElems: Collecting element indices failed");
                object_destroy(indices);
                return NULL;
            }
        }
    }

    return indices;
}

McxStatus Fmu2SetDependencies(Fmu2CommonStruct * fmu2, Databus * db, Dependencies * deps, int init) {
    McxStatus ret_val = RETURN_OK;

    size_t *start_index = NULL;
    size_t *dependency = NULL;
    char   *factor_kind = NULL;

    size_t i = 0, j = 0, k = 0;
    size_t num_dependencies = 0;
    size_t dep_idx = 0;

    // mapping between dependency indices (from the modelDescription file) and the dependency <-> channel mapping
    SizeTSizeTMap *dependencies_to_in_channels = (SizeTSizeTMap*)object_create(SizeTSizeTMap);
    // mapping between unknown indices (from the modelDescription file) and the unknowns <-> channel list
    SizeTSizeTMap *unknowns_to_out_channels = (SizeTSizeTMap*)object_create(SizeTSizeTMap);

    // get dependency information via the fmi library
    fmi2_import_variable_list_t * init_unknowns = NULL;

    if (init) {
        init_unknowns = fmi2_import_get_initial_unknowns_list(fmu2->fmiImport);
        fmi2_import_get_initial_unknowns_dependencies(fmu2->fmiImport, &start_index, &dependency, &factor_kind);
    } else {
        init_unknowns = fmi2_import_get_outputs_list(fmu2->fmiImport);
        fmi2_import_get_outputs_dependencies(fmu2->fmiImport, &start_index, &dependency, &factor_kind);
    }

    size_t num_init_unknowns = fmi2_import_get_variable_list_size(init_unknowns);

    // the dependency information in <InitialUnknowns> is encoded via variable indices in modelDescription.xml
    // our dependency matrix uses channel indices
    // to align those 2 index types we use helper dictionaries which store the mapping between them

    // map each dependency index to an input channel index
    ObjectContainer *in_vars = fmu2->in;
    size_t num_in_vars = in_vars->Size(in_vars);

    DatabusInfo * db_info = DatabusGetInInfo(db);
    size_t num_in_channels = DatabusInfoGetChannelNum(db_info);

    // list describing for each channel which channel elements are connected
    ConnectedElems * in_channel_connectivity = (ConnectedElems *) mcx_calloc(num_in_channels, sizeof(ConnectedElems));
    if (!in_channel_connectivity) {
        mcx_log(LOG_ERROR, "SetDependenciesFMU2: Input connectivity map allocation failed");
        ret_val = RETURN_ERROR;
        goto cleanup;
    }

    // List which for each dependency describes which channel and element index it corresponds to
    // The index of elements in this list doesn't correspond to the dependency index from the modelDescription file
    ChannelElement * dependencies_to_inputs = (ChannelElement *) mcx_calloc(DatabusGetInChannelsElemNum(db), sizeof(ChannelElement));
    if (!dependencies_to_inputs) {
        mcx_log(LOG_ERROR, "SetDependenciesFMU2: Dependencies to inputs map allocation failed");
        ret_val = RETURN_ERROR;
        goto cleanup;
    }

    // list used to know the mapping between unknowns and channels/elements
    ChannelElement * unknowns_to_outputs = (ChannelElement *) mcx_calloc(DatabusGetOutChannelsElemNum(db), sizeof(ChannelElement));
    if (!unknowns_to_outputs) {
        mcx_log(LOG_ERROR, "SetDependenciesFMU2: Unknowns to outputs map allocation failed");
        ret_val = RETURN_ERROR;
        goto cleanup;
    }

    // List used to later find ommitted <Unknown> elements
    ChannelElement * processed_output_elems = (ChannelElement *) mcx_calloc(DatabusGetOutChannelsElemNum(db), sizeof(ChannelElement));
    if (!processed_output_elems) {
        mcx_log(LOG_ERROR, "SetDependenciesFMU2: Processed output elements allocation failed");
        ret_val = RETURN_ERROR;
        goto cleanup;
    }

    for (i = 0; i < num_in_vars; ++i) {
        Fmu2Value *val = (Fmu2Value *)in_vars->At(in_vars, i);
        Channel * ch = (Channel *) DatabusGetInChannel(db, i);
        ChannelIn * in = (ChannelIn *) ch;
        ChannelInfo * info = DatabusInfoGetChannel(db_info, i);
        if (ch->IsConnected(ch)) {
            if (ChannelTypeIsArray(info->type)) {
                in_channel_connectivity[i].is_connected = TRUE;
                in_channel_connectivity[i].num_elems = 0;
                in_channel_connectivity[i].elems = (int *) mcx_calloc(ChannelDimensionNumElements(info->dimension), sizeof(size_t));
                if (!in_channel_connectivity[i].elems) {
                    mcx_log(LOG_ERROR, "SetDependenciesFMU2: Input connectivity element container allocation failed");
                    ret_val = RETURN_ERROR;
                    goto cleanup;
                }

                // if an element index appears in elems, it means that element is connected
                Vector * connInfos = in->GetConnectionInfos(in);
                for (j = 0; j < connInfos->Size(connInfos); j++) {
                    ConnectionInfo * connInfo = *(ConnectionInfo**) connInfos->At(connInfos, j);
                    size_t k = 0;

                    for (k = 0; k < ChannelDimensionNumElements(connInfo->targetDimension); k++) {
                        size_t idx = ChannelDimensionGetIndex(connInfo->targetDimension, k, info->type->ty.a.dims) - info->dimension->startIdxs[0];
                        in_channel_connectivity[i].elems[in_channel_connectivity[i].num_elems++] = idx;
                    }
                }
                object_destroy(connInfos);
            } else {
                in_channel_connectivity[i].is_connected = TRUE;
                // scalar channels are treated like they have 1 element (equal to zero)
                in_channel_connectivity[i].num_elems = 1;
                in_channel_connectivity[i].elems = (int*)mcx_calloc(1, sizeof(size_t));
                if (!in_channel_connectivity[i].elems) {
                    mcx_log(LOG_ERROR, "SetDependenciesFMU2: Input connectivity element allocation failed");
                    ret_val = RETURN_ERROR;
                    goto cleanup;
                }
                in_channel_connectivity[i].elems[0] = 0;
            }
        }

        if (val->data->type == FMU2_VALUE_SCALAR) {
            fmi2_import_variable_t *var = val->data->data.scalar;
            size_t idx = fmi2_import_get_variable_original_order(var) + 1;
            dependencies_to_in_channels->Add(dependencies_to_in_channels, idx, dep_idx);

            dependencies_to_inputs[dep_idx].channel_idx = i;
            dep_idx++;
        } else if (val->data->type == FMU2_VALUE_ARRAY) {
            size_t num_elems = 1;
            size_t j = 0;

            for (j = 0; j < val->data->data.array.numDims; j++) {
                num_elems *= val->data->data.array.dims[j];
            }

            for (j = 0; j < num_elems; j++) {
                fmi2_import_variable_t * var = val->data->data.array.values[j];
                size_t idx = fmi2_import_get_variable_original_order(var) + 1;
                dependencies_to_in_channels->Add(dependencies_to_in_channels, idx, dep_idx);

                dependencies_to_inputs[dep_idx].channel_idx = i;
                dependencies_to_inputs[dep_idx].elem_idx = j;
                dep_idx++;
            }
        }
    }

    // <InitialUnknowns> element is not present in modelDescription.xml
    // The dependency matrix consists of only 1 (if input is connected)
    if (start_index == NULL) {
        for (i = 0; i < GetDependencyNumOut(deps); ++i) {
            for (j = 0; j < GetDependencyNumIn(deps); ++j) {
                if (in_channel_connectivity[j].is_connected) {
                    ret_val = SetDependency(deps, j, i, DEP_DEPENDENT);
                    if (RETURN_OK != ret_val) {
                        goto cleanup;
                    }
                }
            }
        }

        goto cleanup;
    }

    // map each initial_unkown index to an output channel index
    // for array channels, there might be multiple entries initial_unknown_idx -> channel_idx
    ObjectContainer *out_vars = fmu2->out;
    size_t num_out_vars = out_vars->Size(out_vars);
    size_t unknown_idx = 0;
    for (i = 0; i < num_out_vars; ++i) {
        Fmu2Value *val = (Fmu2Value *)out_vars->At(out_vars, i);

        if (val->data->type == FMU2_VALUE_SCALAR) {
            fmi2_import_variable_t *var = val->data->data.scalar;
            size_t idx = fmi2_import_get_variable_original_order(var) + 1;
            unknowns_to_out_channels->Add(unknowns_to_out_channels, idx, unknown_idx);

            unknowns_to_outputs[unknown_idx].channel_idx = i;
            unknown_idx++;
        } else if (val->data->type == FMU2_VALUE_ARRAY) {
            size_t num_elems = 1;
            size_t j = 0;

            for (j = 0; j < val->data->data.array.numDims; j++) {
                num_elems *= val->data->data.array.dims[j];
            }

            for (j = 0; j < num_elems; j++) {
                fmi2_import_variable_t * var = val->data->data.array.values[j];
                size_t idx = fmi2_import_get_variable_original_order(var) + 1;
                unknowns_to_out_channels->Add(unknowns_to_out_channels, idx, unknown_idx);

                unknowns_to_outputs[unknown_idx].channel_idx = i;
                unknowns_to_outputs[unknown_idx].elem_idx = j;
                unknown_idx++;
            }
        }
    }

    // fill up the dependency matrix
    size_t processed_elems = 0;
    for (i = 0; i < num_init_unknowns; ++i) {
        fmi2_import_variable_t *init_unknown = fmi2_import_get_variable(init_unknowns, i);
        size_t init_unknown_idx = fmi2_import_get_variable_original_order(init_unknown) + 1;

        SizeTSizeTElem * out_pair = unknowns_to_out_channels->Get(unknowns_to_out_channels, init_unknown_idx);
        if (out_pair == NULL) {
            continue;      // in case some variables are ommitted from the input file
        }

        ChannelElement * out_elem = &unknowns_to_outputs[out_pair->value];

        processed_output_elems[processed_elems].channel_idx = out_elem->channel_idx;
        processed_output_elems[processed_elems].elem_idx = out_elem->elem_idx;
        processed_elems++;

        num_dependencies = start_index[i + 1] - start_index[i];
        for (j = 0; j < num_dependencies; ++j) {
            dep_idx = dependency[start_index[i] + j];
            if (dep_idx == 0) {
                // The <Unknown> element does not explicitly define a `dependencies` attribute
                // In this case it depends on all inputs
                for (k = 0; k < num_in_channels; ++k) {
                    if (in_channel_connectivity[k].is_connected) {
                        ret_val = SetDependency(deps, k, out_elem->channel_idx, DEP_DEPENDENT);
                        if (RETURN_OK != ret_val) {
                            goto cleanup;
                        }
                    }
                }
            } else {
                // The <Unknown> element explicitly defines its dependencies
                SizeTSizeTElem * in_pair = dependencies_to_in_channels->Get(dependencies_to_in_channels, dep_idx);
                if (in_pair) {
                    ChannelElement * dep = &dependencies_to_inputs[in_pair->value];

                    ConnectedElems * elems = &in_channel_connectivity[dep->channel_idx];
                    if (elems->is_connected) {
                        size_t k = 0;
                        for (k = 0; k < elems->num_elems; k++) {
                            if (elems->elems[k] == dep->elem_idx) {
                                ret_val = SetDependency(deps, dep->channel_idx, out_elem->channel_idx, DEP_DEPENDENT);
                                if (RETURN_OK != ret_val) {
                                    goto cleanup;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Initial unknowns which are ommitted from the <InitialUnknowns> element in
    // modelDescription.xml file depend on all inputs
    for (i = 0; i < num_out_vars; ++i) {
        Fmu2Value * val = (Fmu2Value *) out_vars->At(out_vars, i);

        Vector * elems = GetAllElems(processed_output_elems, processed_elems, i);

        if (val->data->type == FMU2_VALUE_ARRAY) {
            size_t num_elems = 1;
            size_t j = 0;

            for (j = 0; j < val->data->data.array.numDims; j++) {
                num_elems *= val->data->data.array.dims[j];
            }

            for (j = 0; j < num_elems; j++) {
                if (!elems->Contains(elems, &j)) {
                    if (fmi2_import_get_initial(val->data->data.array.values[j]) != fmi2_initial_enu_exact) {
                        for (k = 0; k < num_in_channels; ++k) {
                            if (in_channel_connectivity[k].is_connected) {
                                ret_val = SetDependency(deps, k, i, DEP_DEPENDENT);
                                if (RETURN_OK != ret_val) {
                                    goto cleanup;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            if (elems->Size(elems) == 0) {
                if (fmi2_import_get_initial(val->data->data.scalar) != fmi2_initial_enu_exact) {
                    for (k = 0; k < num_in_channels; ++k) {
                        if (in_channel_connectivity[k].is_connected) {
                            ret_val = SetDependency(deps, k, i, DEP_DEPENDENT);
                            if (RETURN_OK != ret_val) {
                                goto cleanup_1;
                            }
                        }
                    }
                }
            }
        }

        object_destroy(elems);
        continue;

cleanup_1:
        object_destroy(elems);
        goto cleanup;
    }

cleanup:    // free dynamically allocated objects
    object_destroy(dependencies_to_in_channels);
    object_destroy(unknowns_to_out_channels);
    if (in_channel_connectivity) { mcx_free(in_channel_connectivity); }
    if (dependencies_to_inputs) { mcx_free(dependencies_to_inputs); }
    if (unknowns_to_outputs) { mcx_free(unknowns_to_outputs); }
    if (processed_output_elems) { mcx_free(processed_output_elems); }
    fmi2_import_free_variable_list(init_unknowns);

    return ret_val;
}


McxStatus Fmu2SetVariableInitialize(Fmu2CommonStruct * fmu, Fmu2Value * fmuVal) {
    McxStatus status = RETURN_OK;

    Channel * channel = fmuVal->channel;
    if (channel && FALSE == channel->IsDefinedDuringInit(channel)) {
        MCX_DEBUG_LOG("Fmu2SetVariable: %s not set: no new value", fmuVal->name);
        return RETURN_OK;
    }

    return Fmu2SetVariable(fmu, fmuVal);
}

// TODO: move into fmu2value?
McxStatus Fmu2SetVariable(Fmu2CommonStruct * fmu, Fmu2Value * fmuVal) {
    fmi2_status_t status = fmi2_status_ok;

    ChannelValue * const chVal = &fmuVal->val;
    ChannelType * type = ChannelValueType(chVal);

    switch (type->con) {
    case CHANNEL_DOUBLE:
    {
        fmi2_value_reference_t vr[] = {fmuVal->data->vr.scalar};

        status = fmi2_import_set_real(fmu->fmiImport, vr, 1, (const fmi2_real_t *) ChannelValueDataPointer(chVal));

        MCX_DEBUG_LOG("Set %s(%d)=%f", fmuVal->name, vr[0], *(double*)ChannelValueDataPointer(chVal));

        break;
    }
    case CHANNEL_INTEGER:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_set_integer(fmu->fmiImport, vr, 1, (const fmi2_integer_t *) ChannelValueDataPointer(chVal));

        MCX_DEBUG_LOG("Set %s(%d)=%d", fmuVal->name, vr[0], *(int*)ChannelValueDataPointer(chVal));

        break;
    }
    case CHANNEL_BOOL:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_set_boolean(fmu->fmiImport, vr, 1, (const fmi2_boolean_t *) ChannelValueDataPointer(chVal));

        break;
    }
    case CHANNEL_STRING:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_set_string(fmu->fmiImport, vr, 1, (fmi2_string_t *) ChannelValueDataPointer(chVal));

        break;
    }
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
    {
        binary_string * binary = (binary_string *) ChannelValueDataPointer(chVal);

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
    case CHANNEL_ARRAY:
    {
        fmi2_value_reference_t * vrs = fmuVal->data->vr.array.values;
        mcx_array * a = (mcx_array *) ChannelValueDataPointer(&fmuVal->val);

        size_t num = mcx_array_num_elements(a);
        void * vals = a->data;

        if (ChannelTypeEq(a->type, &ChannelTypeDouble)) {
            status = fmi2_import_set_real(fmu->fmiImport, vrs, num, vals);
        } else if (ChannelTypeEq(a->type, &ChannelTypeInteger)) {
            status = fmi2_import_set_integer(fmu->fmiImport, vrs, num, vals);
        } else {
            mcx_log(LOG_ERROR, "FMU: Unsupported array variable type: %s", ChannelTypeToString(a->type));
            return RETURN_ERROR;
        }

        break;
    }
    default:
        mcx_log(LOG_ERROR, "FMU: Unknown variable type: %s", ChannelTypeToString(type));
        return RETURN_ERROR;
    }

    if (fmi2_status_ok != status) {
        if (fmi2_status_error == status || fmi2_status_fatal == status) {
            fmu->runOk = fmi2_false;
            mcx_log(LOG_ERROR, "FMU: Setting of variable %s failed", fmuVal->name);
            return RETURN_ERROR;
        } else {
            if (fmi2_status_warning == status) {
                mcx_log(LOG_WARNING, "FMU: Setting of variable %s return with a warning", fmuVal->name);
            } else if (fmi2_status_discard == status) {
                mcx_log(LOG_WARNING, "FMU: Setting of variable %s discarded", fmuVal->name);
            }
        }
    }

    return RETURN_OK;
}

McxStatus Fmu2SetVariableArray(Fmu2CommonStruct * fmu, ObjectContainer * vals) {
    size_t i = 0;
    size_t numVars = vals->Size(vals);

    McxStatus retVal = RETURN_OK;

    mcx_signal_handler_set_this_function();

    for (i = 0; i < numVars; i++) {
        Fmu2Value * const fmuVal = (Fmu2Value *) vals->At(vals, i);

        retVal = Fmu2SetVariable(fmu, fmuVal);
        if (RETURN_ERROR == retVal) {
            mcx_signal_handler_unset_function();
            return RETURN_ERROR;
        }
    }

    mcx_signal_handler_unset_function();

    return RETURN_OK;
}

McxStatus Fmu2SetVariableArrayInitialize(Fmu2CommonStruct * fmu, ObjectContainer * vals) {
    size_t i = 0;
    size_t numVars = vals->Size(vals);

    McxStatus retVal = RETURN_OK;

    mcx_signal_handler_set_this_function();

    for (i = 0; i < numVars; i++) {
        Fmu2Value * const fmuVal = (Fmu2Value *) vals->At(vals, i);

        retVal = Fmu2SetVariableInitialize(fmu, fmuVal);
        if (RETURN_ERROR == retVal) {
            mcx_signal_handler_unset_function();
            return RETURN_ERROR;
        }
    }

    mcx_signal_handler_unset_function();

    return RETURN_OK;
}

McxStatus Fmu2GetVariable(Fmu2CommonStruct * fmu, Fmu2Value * fmuVal) {
    fmi2_status_t status = fmi2_status_ok;

    char * const name = fmuVal->name;
    ChannelValue * const chVal = &fmuVal->val;

    ChannelType * type = ChannelValueType(chVal);

    switch (type->con) {
    case CHANNEL_DOUBLE:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_get_real(fmu->fmiImport, vr, 1, (fmi2_real_t *) ChannelValueDataPointer(chVal));

        MCX_DEBUG_LOG("Get %s(%d)=%f", fmuVal->name, vr[0], *(double*)ChannelValueDataPointer(chVal));

        break;
    }
    case CHANNEL_INTEGER:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_get_integer(fmu->fmiImport, vr, 1, (fmi2_integer_t *) ChannelValueDataPointer(chVal));

        break;
    }
    case CHANNEL_BOOL:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        status = fmi2_import_get_boolean(fmu->fmiImport, vr, 1, (fmi2_boolean_t *) ChannelValueDataPointer(chVal));

        break;
    }
    case CHANNEL_STRING:
    {
        fmi2_value_reference_t vr[] = { fmuVal->data->vr.scalar };

        char * buffer = NULL;

        status = fmi2_import_get_string(fmu->fmiImport, vr, 1, (fmi2_string_t *) &buffer);
        if (RETURN_OK != ChannelValueSetFromReference(chVal, &buffer)) {
            return RETURN_ERROR;
        }

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

        if (RETURN_OK != ChannelValueSetFromReference(chVal, &binary)) {
            return RETURN_ERROR;
        }

        break;
    }
    case CHANNEL_ARRAY:
    {
        fmi2_value_reference_t * vrs = fmuVal->data->vr.array.values;
        mcx_array * a = (mcx_array *) ChannelValueDataPointer(&fmuVal->val);

        size_t num = mcx_array_num_elements(a);
        void * vals = a->data;

        if (ChannelTypeEq(a->type, &ChannelTypeDouble)) {
            status = fmi2_import_get_real(fmu->fmiImport, vrs, num, vals);
        } else if (ChannelTypeEq(a->type, &ChannelTypeInteger)) {
            status = fmi2_import_get_integer(fmu->fmiImport, vrs, num, vals);
        } else {
            // TODO: log message
            return RETURN_ERROR;
        }

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

    mcx_signal_handler_set_this_function();

    for (i = 0; i < numVars; i++) {
        Fmu2Value * const fmuVal = (Fmu2Value *) vals->At(vals, i);

        retVal = Fmu2GetVariable(fmu, fmuVal);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "FMU: Getting of variable array failed at element %u", i);
            mcx_signal_handler_unset_function();
            return RETURN_ERROR;
        }
    }

    mcx_signal_handler_unset_function();

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
        ChannelType * type = ChannelValueType(&val->val);

        if (ChannelTypeEq(&ChannelTypeDouble, type)) {
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

        retVal = DatabusAddLocalChannel(db, name, buffer, unitName, ChannelValueDataPointer(&val->val), ChannelValueType(&val->val));
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
        ChannelType * type = Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var));
        fmi2_import_unit_t * unit = NULL;
        const char * unitName = NULL;

        if (ChannelTypeEq(&ChannelTypeDouble, type)) {
            unit = fmi2_import_get_real_variable_unit(fmi2_import_get_variable_as_real(var));
            if (unit) {
                unitName = fmi2_import_get_unit_name(unit);
            }
        }

        Fmu2Value * value = Fmu2ValueScalarMake(name, var, unitName, NULL);
        if (value) {
            list->PushBackNamed(list, (Object *) value, name);
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
        ChannelType * type = Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(var));
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
        } else if (val->data->type == FMU2_VALUE_ARRAY) {
            size_t num_elems = 1;
            size_t j = 0;
            int all_tunable = TRUE;

            for (j = 0; j < val->data->data.array.numDims; j++) {
                num_elems *= val->data->data.array.dims[j];
            }

            for (j = 0; j < num_elems; j++) {
                fmi2_import_variable_t * var = val->data->data.array.values[j];
                fmi2_causality_enu_t causality = fmi2_import_get_causality(var);

                if (fmi2_causality_enu_input == causality) {
                    all_tunable = FALSE;
                    break;
                }
            }

            if (all_tunable) {
                ChannelIn * in = (ChannelIn *) val->channel;
                ChannelInfo * info = &((Channel *) in)->info;
                mcx_log(LOG_DEBUG, "Setting input \"%s\" as discrete", ChannelInfoGetLogName(info));
                in->SetDiscrete(in);
            }
        }
    }
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */