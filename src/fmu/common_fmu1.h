/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_FMU_COMMON_FMU1_H
#define MCX_FMU_COMMON_FMU1_H

#include "core/Databus.h" //needed to add local variables to databus

#include "reader/model/components/specific_data/FmuInput.h"
#include "reader/model/parameters/ParametersInput.h"

#include "fmu/common_fmu.h"
#include "fmu/Fmu1Value.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum Fmu1Type {
    Fmu1CoSimulation,
} Fmu1Type;

typedef enum enum_VAR_TYPE {
    VAR_UNDEFINED = -1,
    VAR_REAL = 1,
    VAR_INT,
    VAR_BOOL,
    VAR_ENUM,
    VAR_STRING
} FMI_VAR_TYPE;

ChannelType * Fmi1TypeToChannelType(fmi1_base_type_enu_t type);


typedef struct Fmu1CommonStruct {
    fmi1_import_t * fmiImport;
    fmi1_boolean_t isLogging;

    fmi1_boolean_t instantiateOk;
    fmi1_boolean_t runOk;

    ObjectContainer * in;
    ObjectContainer * out;
    ObjectContainer * params;
    ObjectContainer * initialValues;

    ObjectContainer * localValues;

    fmi1_integer_t * int32Buffer;
    size_t int32BufferSize;
} Fmu1CommonStruct;

McxStatus Fmu1CommonStructInit(Fmu1CommonStruct * fmu);
McxStatus Fmu1CommonStructRead(FmuCommon * common, Fmu1CommonStruct * fmu1, Fmu1Type fmu_type, FmuInput * input);
McxStatus Fmu1CommonStructSetup(FmuCommon * common, Fmu1CommonStruct * fmu1, Fmu1Type fmu_type);
void Fmu1CommonStructDestructor(Fmu1CommonStruct * fmu);

ObjectContainer * Fmu1ReadParams(ParametersInput * input, fmi1_import_t * import, ObjectContainer * ignore);

McxStatus Fmu1SetupInt32Buffer(Fmu1CommonStruct * fmu);

McxStatus Fmu1SetVariableArray(Fmu1CommonStruct * fmu, ObjectContainer * vals);
McxStatus Fmu1GetVariableArray(Fmu1CommonStruct * fmu, ObjectContainer * vals);

McxStatus Fmu1SetVariable(Fmu1CommonStruct * fmu, Fmu1Value * fmuVal);
McxStatus Fmu1GetVariable(Fmu1CommonStruct * fmu, Fmu1Value * fmuVal);

int fmi1FilterLocalVariables(fmi1_import_variable_t *vl, void *data);

McxStatus fmi1CreateValuesOutOfVariables(ObjectContainer * vals, fmi1_import_variable_list_t * vars);

McxStatus fmi1AddLocalChannelsFromLocalValues(ObjectContainer * vals, const char * compName, Databus * db);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_FMU_COMMON_FMU1_H */