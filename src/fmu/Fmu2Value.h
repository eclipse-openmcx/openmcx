/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_FMU_FMU2VALUE_H
#define MCX_FMU_FMU2VALUE_H

#include "CentralParts.h"
#include "core/channels/Channel.h"
#include "fmilib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct Fmu2ValueData;

typedef struct Fmu2ValueData Fmu2ValueData;

extern const struct ObjectClass _Fmu2ValueData;

typedef enum Fmu2ValueType {
    FMU2_VALUE_SCALAR
    , FMU2_VALUE_BINARY_OSI
    , FMU2_VALUE_INVALID
} Fmu2ValueType;

struct Fmu2ValueData {
    Object _; /* base class */

    Fmu2ValueType type;
    union {
        fmi2_import_variable_t * scalar;
        struct {
            fmi2_import_variable_t * lo;
            fmi2_import_variable_t * hi;
            fmi2_import_variable_t * size;
        } binary;
    } data;
    union {
        fmi2_value_reference_t scalar;
        struct {
            fmi2_value_reference_t lo;
            fmi2_value_reference_t hi;
            fmi2_value_reference_t size;
        } binary;
    } vr;
};

Fmu2ValueData * Fmu2ValueDataScalarMake(fmi2_import_variable_t * scalar);
Fmu2ValueData * Fmu2ValueDataBinaryMake(fmi2_import_variable_t * hi, fmi2_import_variable_t * lo, fmi2_import_variable_t * size);


extern const struct ObjectClass _Fmu2VariableInfo;

typedef struct Fmu2VariableInfo {
    Object _;

    ChannelType type;

    ChannelValueData * min;
    ChannelValueData * max;

    char * desc;
} Fmu2VariableInfo;


struct Fmu2Value;

typedef struct Fmu2Value Fmu2Value;

typedef McxStatus (* fFmu2ValueSetFromChannelValue)(Fmu2Value * v, ChannelValue * val);
typedef McxStatus (* fFmu2ValueSetup)(Fmu2Value * v, const char * name, Fmu2ValueData * data, const char * unit, Channel * channel);
typedef void (* fFmu2ValueSetChannel)(Fmu2Value * v, Channel * channel);

extern const struct ObjectClass _Fmu2Value;

struct Fmu2Value {
    Object _; /* base class */

    fFmu2ValueSetFromChannelValue SetFromChannelValue;
    fFmu2ValueSetup Setup;
    fFmu2ValueSetChannel SetChannel;

    char * name;
    char * unit;

    Channel * channel;
    Fmu2ValueData * data;
    Fmu2VariableInfo * info;
    ChannelValue val;
};

Fmu2Value * Fmu2ValueMake(const char * name, Fmu2ValueData * data, const char * unit, Channel * channel);
Fmu2Value * Fmu2ValueScalarMake(const char * name, fmi2_import_variable_t * scalar, const char * unit, Channel * channel);
Fmu2Value * Fmu2ValueBinaryMake(const char * name, fmi2_import_variable_t * hi, fmi2_import_variable_t * lo, fmi2_import_variable_t * size, Channel * channel);

void Fmu2ValuePrintDebug(Fmu2Value * val);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_FMU_FMU2VALUE_H */