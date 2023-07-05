/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_FMU_FMU1VALUE_H
#define MCX_FMU_FMU1VALUE_H

#include "CentralParts.h"
#include "core/channels/Channel.h"
#include "fmilib.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef enum Fmu1ValueType {
    FMU1_VALUE_SCALAR,
    FMU1_VALUE_ARRAY,
    FMU1_VALUE_INVALID
} Fmu1ValueType;

extern const struct ObjectClass _Fmu1ValueData;

typedef struct Fmu1ValueData {
    Object _;

    Fmu1ValueType type;
    union {
        fmi1_import_variable_t * scalar;
        struct {
            size_t numDims;
            size_t * dims;
            fmi1_import_variable_t ** values;
        } array;
    } var;
    union {
        fmi1_value_reference_t scalar;
        struct {
            fmi1_value_reference_t * values;
        } array;
    } vr;
} Fmu1ValueData;

size_t Fmu1ValueDataArrayNumElems(const Fmu1ValueData * data);

struct Fmu1Value;

typedef struct Fmu1Value Fmu1Value;

typedef McxStatus(*fFmu1ValueSetFromChannelValue)(Fmu1Value * v, ChannelValue * val);
typedef McxStatus (*fFmu1ValueSetup)(Fmu1Value * value, const char * name, Fmu1ValueData * data, Channel * channel);

extern const struct ObjectClass _Fmu1Value;

struct Fmu1Value {
    Object _; /* base class */

    fFmu1ValueSetFromChannelValue SetFromChannelValue;
    fFmu1ValueSetup Setup;

    char * name;

    Channel * channel;
    Fmu1ValueData * data;
    ChannelValue val;
};

Fmu1Value * Fmu1ValueScalarMake(const char * name, fmi1_import_variable_t * var, Channel * channel);
Fmu1Value * Fmu1ValueArrayMake(const char * name, size_t numDims, size_t * dims, fmi1_import_variable_t ** vars, Channel * channel);

Fmu1Value * Fmu1ValueReadScalar(const char * logPrefix,
                                ChannelType * type,
                                Channel * channel,
                                const char * channelName,
                                fmi1_import_t * fmiImport);
Fmu1Value * Fmu1ValueReadArray(const char * logPrefix,
                               ChannelType * type,
                               Channel * channel,
                               const char * channelName,
                               ChannelDimension * dimension,
                               fmi1_import_t * fmiImport);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_FMU_FMU1VALUE_H */