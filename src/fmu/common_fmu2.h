/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_FMU_COMMON_FMU2_H
#define MCX_FMU_COMMON_FMU2_H

#include "fmu/common_fmu.h"
#include "JM/jm_callbacks.h"
#include "core/Databus.h" //needed to add local variables to databus
#include "fmu/Fmu2Value.h"

#include "reader/model/components/specific_data/FmuInput.h"
#include "reader/model/parameters/ParametersInput.h"

#include "core/Dependency.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

ChannelType * Fmi2TypeToChannelType(fmi2_base_type_enu_t type);
const char * Fmi2TypeToString(fmi2_base_type_enu_t type);

struct Fmu2CommonStruct;

typedef struct Fmu2CommonStruct Fmu2CommonStruct;

McxStatus Fmu2ReadParams(ObjectContainer * params, ObjectContainer * arrayParams, ParametersInput * input, fmi2_import_t * import, ObjectContainer * ignore);

McxStatus Fmu2SetDependencies(Fmu2CommonStruct * fmu2, Databus * db, Dependencies * deps, int init);

// TODO: rename all variablearrays to something better
McxStatus Fmu2SetVariableArray(Fmu2CommonStruct * fmu, ObjectContainer * vals);
McxStatus Fmu2GetVariableArray(Fmu2CommonStruct * fmu, ObjectContainer * vals);

McxStatus Fmu2SetVariable(Fmu2CommonStruct * fmu, Fmu2Value * fmuVal);
McxStatus Fmu2GetVariable(Fmu2CommonStruct * fmu, Fmu2Value * fmuVal);

McxStatus Fmi2RegisterLocalChannelsAtDatabus(ObjectContainer * vals, const char * compName, Databus * db);

struct Fmu2CommonStruct {
    fmi2_import_t * fmiImport;
    fmi2_boolean_t isLogging;

    fmi2_boolean_t instantiateOk;
    fmi2_boolean_t runOk;

    ObjectContainer * in;
    ObjectContainer * out;
    ObjectContainer * params;
    ObjectContainer * initialValues;

    ObjectContainer * arrayParams;   // proxy views to array parameter elements (from params) (type: ArrayParameterProxy)

    ObjectContainer * connectedIn;

    ObjectContainer * localValues;
    ObjectContainer * tunableParams;

    size_t numLogCategories;
    fmi2_string_t * logCategories;

};

McxStatus Fmu2CommonStructInit(Fmu2CommonStruct * fmu);
McxStatus Fmu2CommonStructRead(FmuCommon * common, Fmu2CommonStruct * fmu2, fmi2_type_t fmu_type, FmuInput * input);
McxStatus Fmu2CommonStructSetup(FmuCommon * common, Fmu2CommonStruct * fmu2, fmi2_type_t fmu_type);
void Fmu2CommonStructDestructor(Fmu2CommonStruct * fmu);

int Fmu2ValueIsContainedInObjectContainerPred(Object * obj, void * ctx);
int Fmu2ValueIsNotContainedInObjectContainerPred(Object * obj, void * ctx);
int fmi2FilterLocalVariables(fmi2_import_variable_t *vl, void *data);
int fmi2FilterTunableParameters(fmi2_import_variable_t *vl, void *data);

McxStatus Fmu2UpdateTunableParamValues(ObjectContainer * tunableParams, ObjectContainer * params);

ObjectContainer * Fmu2ValueScalarListFromVarList(fmi2_import_variable_list_t * vars);
ObjectContainer * Fmu2ValueScalarListFromValVarList(ObjectContainer * vals, fmi2_import_variable_list_t * vars);
fmi2_import_variable_list_t * Fmu2GetFilteredVars(fmi2_import_t * import, int (* filter)(fmi2_import_variable_t *vl, void *data));
ObjectContainer * Fmu2ReadTunableParams(fmi2_import_t * import);
ObjectContainer * Fmu2ReadLocalVariables(fmi2_import_t * import);

McxStatus Fmu2CheckTunableParamsInputConsistency(ObjectContainer * in, ObjectContainer * params,  ObjectContainer * tunableParams);
void Fmu2MarkTunableParamsAsInputAsDiscrete(ObjectContainer * in);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_FMU_COMMON_FMU2_H