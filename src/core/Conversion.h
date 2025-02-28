/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONVERSION_H
#define MCX_CORE_CONVERSION_H

#include "units/Units.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct Conversion Conversion;

typedef McxStatus (* fConversion)(Conversion * conversion, ChannelValue * value);

extern const struct ObjectClass _Conversion;

struct Conversion {
    Object _; /* super class first */

    /* Applies the conversion to its value argument. */
    fConversion convert;
} ;


// ----------------------------------------------------------------------
// Range Conversion

typedef struct RangeConversion RangeConversion;

typedef McxStatus (* fRangeConversionSetup)(RangeConversion * conversion, ChannelValue * min, ChannelValue * max);
typedef int (* fRangeConversionIsEmpty)(RangeConversion * conversion);

extern const struct ObjectClass _RangeConversion;

typedef struct RangeConversion {
    Conversion _;

    fRangeConversionSetup Setup;
    fRangeConversionIsEmpty IsEmpty;

    ChannelType type;

    ChannelValue * min;
    ChannelValue * max;

} RangeConversion;

McxStatus ConvertRange(ChannelValue * min, ChannelValue * max, ChannelValue * value);


// ----------------------------------------------------------------------
// Unit Conversion
typedef struct UnitConversion UnitConversion;

typedef void(*fUnitConversionVector)(UnitConversion * conversion, double * value, size_t vectorLength);

typedef McxStatus (* fUnitConversionSetup)(UnitConversion * conversion, const char * fromUnit, const char * toUnit);
typedef int (* fUnitConversionIsEmpty)(UnitConversion * conversion);

extern const struct ObjectClass _UnitConversion;

struct UnitConversion {
    Conversion _;

    fUnitConversionSetup Setup;
    fUnitConversionIsEmpty IsEmpty;

    si_def source;
    si_def target;

    fUnitConversionVector convertVector;
};

McxStatus ConvertUnit(const char * fromUnit, const char * toUnit, ChannelValue * value);


// ----------------------------------------------------------------------
// Linear Conversion

typedef struct LinearConversion LinearConversion;

typedef McxStatus (* fLinearConversionSetup)(LinearConversion * conversion, ChannelValue * factor, ChannelValue * offset);
typedef int (* fLinearConversionIsEmpty)(LinearConversion * conversion);

extern const struct ObjectClass _LinearConversion;

typedef struct LinearConversion {
    Conversion _;

    fLinearConversionSetup Setup;
    fLinearConversionIsEmpty IsEmpty;

    ChannelType type;

    ChannelValue * factor;
    ChannelValue * offset;

} LinearConversion;

McxStatus ConvertLinear(ChannelValue * factor, ChannelValue * offset, ChannelValue * value);


// ----------------------------------------------------------------------
// Type Conversion

typedef struct TypeConversion TypeConversion;

typedef McxStatus (* fTypeConversionSetup)(TypeConversion * conversion, ChannelType fromType, ChannelType toType);
typedef int (* fTypeConversionIsEmpty)(TypeConversion * conversion);

extern const struct ObjectClass _TypeConversion;

struct TypeConversion {
    Conversion _;

    fTypeConversionSetup Setup;
    fTypeConversionIsEmpty IsEmpty;
};

McxStatus ConvertType(ChannelType fromType, ChannelType toType, ChannelValue * value);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONVERSION_H */