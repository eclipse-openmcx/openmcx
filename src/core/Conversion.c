/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"
#include "core/Conversion.h"
#include "units/Units.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ----------------------------------------------------------------------
// Conversion

static void ConversionDestructor(Conversion * conversion) {

}

static Conversion * ConversionCreate(Conversion * conversion) {
    conversion->convert = NULL;

    return conversion;
}

OBJECT_CLASS(Conversion, Object);


// ----------------------------------------------------------------------
// Range Conversion

McxStatus ConvertRange(ChannelValue * min, ChannelValue * max, ChannelValue * value) {
    RangeConversion * rangeConv = NULL;
    ChannelValue * minToUse = NULL;
    ChannelValue * maxToUse = NULL;

    McxStatus retVal = RETURN_OK;

    if (!ChannelTypeEq(value->type, &ChannelTypeDouble) && !ChannelTypeEq(value->type, &ChannelTypeInteger)) {
        return RETURN_OK;
    }

    rangeConv = (RangeConversion *) object_create(RangeConversion);
    if (!rangeConv) {
        mcx_log(LOG_ERROR, "ConvertRange: Not enough memory");
        return RETURN_ERROR;
    }

    maxToUse = max ? ChannelValueClone(max) : NULL;
    if (max && !maxToUse) {
        mcx_log(LOG_ERROR, "ConvertRange: Not enough memory for max");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    minToUse = min ? ChannelValueClone(min) : NULL;
    if (min && !minToUse) {
        mcx_log(LOG_ERROR, "ConvertRange: Not enough memory for min");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    retVal = rangeConv->Setup(rangeConv, minToUse, maxToUse);
    if (retVal == RETURN_ERROR) {
        mcx_log(LOG_ERROR, "ConvertRange: Conversion setup failed");
        goto cleanup;
    }

    minToUse = NULL;
    maxToUse = NULL;

    if (rangeConv->IsEmpty(rangeConv)) {
        object_destroy(rangeConv);
    }

    if (rangeConv) {
        Conversion * conversion = (Conversion *) rangeConv;
        retVal = conversion->convert(conversion, value);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "ConvertRange: Conversion failed");
            goto cleanup;
        }
    }

cleanup:
    if (minToUse) {
        mcx_free(minToUse);
    }

    if (maxToUse) {
        mcx_free(maxToUse);
    }

    object_destroy(rangeConv);

    return retVal;
}

static McxStatus RangeConversionConvert(Conversion * conversion, ChannelValue * value) {
    RangeConversion * rangeConversion = (RangeConversion *) conversion;

    McxStatus retVal = RETURN_OK;

    if (!ChannelTypeEq(ChannelValueType(value), rangeConversion->type)) {
        mcx_log(LOG_ERROR, "Range conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(rangeConversion->type));
        return RETURN_ERROR;
    }

    if (rangeConversion->min && ChannelValueLeq(value, rangeConversion->min)) {
        retVal = ChannelValueSet(value, rangeConversion->min);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Range conversion: Set value to min failed");
            return RETURN_ERROR;
        }
    } else if (rangeConversion->max && ChannelValueGeq(value, rangeConversion->max)) {
        retVal = ChannelValueSet(value, rangeConversion->max);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Range conversion: Set value to max failed");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus RangeConversionSetup(RangeConversion * conversion, ChannelValue * min, ChannelValue * max) {
    if (!min && !max) {
        return RETURN_OK;
    }

    if (min && max && !ChannelTypeEq(ChannelValueType(min), ChannelValueType(max))) {
        mcx_log(LOG_ERROR, "Range conversion: Types of max value and min value do not match");
        return RETURN_ERROR;
    }

    if (min && max && !ChannelValueLeq(min, max)) {
        mcx_log(LOG_ERROR, "Range conversion: Specified max. value < specified min. value");
        return RETURN_ERROR;
    }

    if (min) {
        conversion->type = ChannelValueType(min);
    } else {
        conversion->type = ChannelValueType(max);
    }

    if (!(ChannelTypeEq(conversion->type, &ChannelTypeDouble)
          || ChannelTypeEq(conversion->type, &ChannelTypeInteger)
          || ChannelTypeIsArray(conversion->type))) {
        mcx_log(LOG_ERROR, "Range conversion is not defined for type %s", ChannelTypeToString(conversion->type));
        return RETURN_ERROR;
    }

    conversion->min = min ? ChannelValueClone(min) : NULL;
    conversion->max = max ? ChannelValueClone(max) : NULL;

    return RETURN_OK;
}

static int RangeConversionIsEmpty(RangeConversion * conversion) {
    switch (conversion->type->con) {
    case CHANNEL_DOUBLE:
        return
            (!conversion->min || * (double *) ChannelValueReference(conversion->min) == (-DBL_MAX)) &&
            (!conversion->max || * (double *) ChannelValueReference(conversion->max) ==   DBL_MAX);
    case CHANNEL_INTEGER:
        return
            (!conversion->min || * (int *) ChannelValueReference(conversion->min) == INT_MIN) &&
            (!conversion->max || * (int *) ChannelValueReference(conversion->max) == INT_MAX);
    default:
        return 1;
    }
}

static void RangeConversionDestructor(RangeConversion * rangeConversion) {
    if (rangeConversion->min) {
        mcx_free(rangeConversion->min);
    }
    if (rangeConversion->max) {
        mcx_free(rangeConversion->max);
    }
}

static RangeConversion * RangeConversionCreate(RangeConversion * rangeConversion) {
    Conversion * conversion = (Conversion *) rangeConversion;

    conversion->convert  = RangeConversionConvert;

    rangeConversion->Setup = RangeConversionSetup;
    rangeConversion->IsEmpty = RangeConversionIsEmpty;

    rangeConversion->type = &ChannelTypeUnknown;

    rangeConversion->min = NULL;
    rangeConversion->max = NULL;

    return rangeConversion;
}

OBJECT_CLASS(RangeConversion, Conversion);


// ----------------------------------------------------------------------
// Unit Conversion

McxStatus ConvertUnit(const char * fromUnit, const char * toUnit, ChannelValue * value) {
    UnitConversion * unitConv = NULL;

    McxStatus retVal = RETURN_OK;

    unitConv = (UnitConversion *) object_create(UnitConversion);
    if (!unitConv) {
        mcx_log(LOG_ERROR, "ConvertUnit: Not enough memory");
        return RETURN_ERROR;
    }

    retVal = unitConv->Setup(unitConv, fromUnit, toUnit);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ConvertUnit: Conversion setup failed");
        goto cleanup;
    }

    if (unitConv->IsEmpty(unitConv)) {
        object_destroy(unitConv);
    }

    if (unitConv) {
        Conversion * conv = (Conversion *) unitConv;
        retVal = conv->convert(conv, value);
        if (retVal == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "ConvertUnit: Conversion failed");
            goto cleanup;
        }
    }

cleanup:
    object_destroy(unitConv);

    return retVal;
}

static void UnitConversionConvertVector(UnitConversion * unitConversion, double * vector, size_t vectorLength) {
    size_t i;
    if (!unitConversion->IsEmpty(unitConversion)) {
        for (i = 0; i < vectorLength; i++) {
            double val = vector[i];

            val = (val + unitConversion->source.offset) * unitConversion->source.factor;
            val = (val / unitConversion->target.factor) - unitConversion->target.offset;

            vector[i] = val;
        }
    }
}

static McxStatus UnitConversionConvert(Conversion * conversion, ChannelValue * value) {
    UnitConversion * unitConversion = (UnitConversion *) conversion;

    double val = 0.0;

    if (!ChannelTypeEq(ChannelValueType(value), &ChannelTypeDouble)) {
        mcx_log(LOG_ERROR, "Unit conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeDouble));
        return RETURN_ERROR;
    }

    val = * (double *) ChannelValueReference(value);

    val = (val + unitConversion->source.offset) * unitConversion->source.factor;
    val = (val / unitConversion->target.factor) - unitConversion->target.offset;

    if (RETURN_OK != ChannelValueSetFromReference(value, &val)) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus UnitConversionSetup(UnitConversion * conversion,
                                     const char * fromUnit,
                                     const char * toUnit) {

    if (fromUnit && toUnit) {
        if (!strcmp(fromUnit, toUnit)) {
            return RETURN_OK;
        }
    }

    if (fromUnit && !strcmp(fromUnit, DEFAULT_NO_UNIT)) {
        fromUnit = NULL;
    }
    if (toUnit && !strcmp(toUnit, DEFAULT_NO_UNIT)) {
        toUnit = NULL;
    }

    if (fromUnit) {
        int status = mcx_units_get_si_def(fromUnit, &conversion->source);
        if (status) {
            mcx_log(LOG_WARNING, "Unit conversion: Unknown unit string \"%s\", ignoring", fromUnit);
        }
    }
    if (toUnit) {
        int status = mcx_units_get_si_def(toUnit, &conversion->target);
        if (status) {
            mcx_log(LOG_WARNING, "Unit conversion: Unknown unit string \"%s\", ignoring", toUnit);
        }
    }

    return RETURN_OK;
}

static int UnitConversionIsEmpty(UnitConversion * conversion) {
    return (conversion->source.factor == 0.0 && conversion->source.offset == 0.0)
        || (conversion->target.factor == 0.0 && conversion->target.offset == 0.0);
}

static void UnitConversionDestructor(UnitConversion * conversion) {

}

static UnitConversion * UnitConversionCreate(UnitConversion * unitConversion) {
    Conversion * conversion = (Conversion *) unitConversion;

    conversion->convert = UnitConversionConvert;
    unitConversion->convertVector = UnitConversionConvertVector;

    unitConversion->Setup   = UnitConversionSetup;
    unitConversion->IsEmpty = UnitConversionIsEmpty;

    unitConversion->source.factor = 0.0;
    unitConversion->source.offset = 0.0;

    unitConversion->target.factor = 0.0;
    unitConversion->target.offset = 0.0;

    return unitConversion;
}

OBJECT_CLASS(UnitConversion, Conversion);


// ----------------------------------------------------------------------
// Linear Conversion

McxStatus ConvertLinear(ChannelValue * factor, ChannelValue * offset, ChannelValue * value) {
    LinearConversion * linearConv = NULL;
    ChannelValue * factorToUse = NULL;
    ChannelValue * offsetToUse = NULL;

    McxStatus retVal = RETURN_OK;

    if (!ChannelTypeEq(value->type, &ChannelTypeDouble) && !ChannelTypeEq(value->type, &ChannelTypeInteger)) {
        return RETURN_OK;
    }

    linearConv = (LinearConversion *) object_create(LinearConversion);
    if (!linearConv) {
        mcx_log(LOG_ERROR, "ConvertLinear: Not enough memory");
        return RETURN_ERROR;
    }

    factorToUse = factor ? ChannelValueClone(factor) : NULL;
    if (factor && !factorToUse) {
        mcx_log(LOG_ERROR, "ConvertLinear: Not enough memory for factor");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    offsetToUse = offset ? ChannelValueClone(offset) : NULL;
    if (offset && !offsetToUse) {
        mcx_log(LOG_ERROR, "ConvertLinear: Not enough memory for offset");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    retVal = linearConv->Setup(linearConv, factorToUse, offsetToUse);
    if (retVal == RETURN_ERROR) {
        mcx_log(LOG_ERROR, "ConvertLinear: Conversion setup failed");
        goto cleanup;
    }

    factorToUse = NULL;
    offsetToUse = NULL;

    if (linearConv->IsEmpty(linearConv)) {
        object_destroy(linearConv);
    }

    if (linearConv) {
        Conversion * conversion = (Conversion *) linearConv;
        retVal = conversion->convert(conversion, value);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "ConvertLinear: Conversion failed");
            goto cleanup;
        }
    }

cleanup:
    if (factorToUse) {
        mcx_free(factorToUse);
    }

    if (offsetToUse) {
        mcx_free(offsetToUse);
    }

    object_destroy(linearConv);

    return retVal;
}

static McxStatus LinearConversionConvert(Conversion * conversion, ChannelValue * value) {
    LinearConversion * linearConversion = (LinearConversion *) conversion;

    McxStatus retVal = RETURN_OK;

    if (linearConversion->factor) {
        retVal = ChannelValueScale(value, linearConversion->factor);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Linear conversion: Port value scaling failed");
            return RETURN_ERROR;
        }
    }

    if (linearConversion->offset) {
        retVal = ChannelValueAddOffset(value, linearConversion->offset);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Linear conversion: Adding offset failed");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus LinearConversionSetup(LinearConversion * conversion, ChannelValue * factor, ChannelValue * offset) {
    if (!factor && !offset) {
        return RETURN_OK;
    }

    if (factor && offset && !ChannelTypeEq(ChannelValueType(factor), ChannelValueType(offset))) {
        mcx_log(LOG_WARNING, "Linear conversion: Types of factor value (%s) and offset value (%s) do not match",
            ChannelTypeToString(ChannelValueType(factor)), ChannelTypeToString(ChannelValueType(offset)));
        return RETURN_ERROR;
    }

    if (factor) {
        conversion->type = ChannelValueType(factor);
    } else {
        conversion->type = ChannelValueType(offset);
    }

    if (!(ChannelTypeEq(conversion->type, &ChannelTypeDouble)
          || ChannelTypeEq(conversion->type, &ChannelTypeInteger)
          || ChannelTypeIsArray(conversion->type))) {
        mcx_log(LOG_WARNING, "Linear conversion is not defined for type %s", ChannelTypeToString(conversion->type));
        return RETURN_ERROR;
    }

    conversion->factor = factor ? ChannelValueClone(factor) : NULL;
    conversion->offset = offset ? ChannelValueClone(offset) : NULL;

    return RETURN_OK;
}

static int LinearConversionIsEmpty(LinearConversion * conversion) {
    switch (conversion->type->con) {
    case CHANNEL_DOUBLE:
        return
            (!conversion->factor || * (double *) ChannelValueReference(conversion->factor) == 1.0) &&
            (!conversion->offset || * (double *) ChannelValueReference(conversion->offset) == 0.0);
    case CHANNEL_INTEGER:
        return
            (!conversion->factor || * (int *) ChannelValueReference(conversion->factor) == 1) &&
            (!conversion->offset || * (int *) ChannelValueReference(conversion->offset) == 0);
    default:
        return 1;
    }
}

static void LinearConversionDestructor(LinearConversion * linearConversion) {
    if (linearConversion->factor) {
        mcx_free(linearConversion->factor);
    }
    if (linearConversion->offset) {
        mcx_free(linearConversion->offset);
    }
}

static LinearConversion * LinearConversionCreate(LinearConversion * linearConversion) {
    Conversion * conversion = (Conversion *) linearConversion;

    conversion->convert  = LinearConversionConvert;
    linearConversion->Setup = LinearConversionSetup;
    linearConversion->IsEmpty = LinearConversionIsEmpty;

    linearConversion->type = &ChannelTypeUnknown;

    linearConversion->factor = NULL;
    linearConversion->offset = NULL;

    return linearConversion;
}

OBJECT_CLASS(LinearConversion, Conversion);


// ----------------------------------------------------------------------
// Type Conversion

McxStatus ConvertType(ChannelType * fromType, ChannelType * toType, ChannelValue * value) {
    TypeConversion * typeConv = NULL;
    Conversion * conv = NULL;

    McxStatus retVal = RETURN_OK;

    typeConv = (TypeConversion *) object_create(TypeConversion);
    if (!typeConv) {
        mcx_log(LOG_ERROR, "ConvertType: Not enough memory");
        return RETURN_ERROR;
    }

    conv = (Conversion *) typeConv;

    retVal = typeConv->Setup(typeConv, fromType, toType);
    if (retVal == RETURN_ERROR) {
        mcx_log(LOG_ERROR, "ConvertType: Conversion setup failed");
        goto cleanup;
    }

    retVal = conv->convert(conv, value);
    if (retVal == RETURN_ERROR) {
        mcx_log(LOG_ERROR, "Type conversion failed");
        goto cleanup;
    }

cleanup:
    object_destroy(typeConv);

    return retVal;
}

static McxStatus TypeConversionConvertIntDouble(Conversion * conversion, ChannelValue * value) {
    if (!ChannelTypeEq(ChannelValueType(value), &ChannelTypeInteger)) {
        mcx_log(LOG_ERROR, "Type conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeInteger));
        return RETURN_ERROR;
    }

    value->type = &ChannelTypeDouble;
    value->value.d = (double)value->value.i;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertDoubleInt(Conversion * conversion, ChannelValue * value) {
    if (!ChannelTypeEq(ChannelValueType(value), &ChannelTypeDouble)) {
        mcx_log(LOG_ERROR, "Type conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeDouble));
        return RETURN_ERROR;
    }

    value->type = &ChannelTypeInteger;
    value->value.i = (int)floor(value->value.d + 0.5);

    return RETURN_OK;
}

static McxStatus TypeConversionConvertBoolDouble(Conversion * conversion, ChannelValue * value) {
    if (!ChannelTypeEq(ChannelValueType(value), &ChannelTypeBool)) {
        mcx_log(LOG_ERROR, "Type conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeBool));
        return RETURN_ERROR;
    }

    value->type = &ChannelTypeDouble;
    value->value.d = (value->value.i != 0) ? 1. : 0.;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertDoubleBool(Conversion * conversion, ChannelValue * value) {
    if (!ChannelTypeEq(ChannelValueType(value), &ChannelTypeDouble)) {
        mcx_log(LOG_ERROR, "Type conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeDouble));
        return RETURN_ERROR;
    }

    value->type = &ChannelTypeBool;
    value->value.i = (value->value.d > 0) ? 1 : 0;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertBoolInteger(Conversion * conversion, ChannelValue * value) {
    if (!ChannelTypeEq(ChannelValueType(value), &ChannelTypeBool)) {
        mcx_log(LOG_ERROR, "Type conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeBool));
        return RETURN_ERROR;
    }

    value->type = &ChannelTypeInteger;
    value->value.i = (value->value.i != 0) ? 1 : 0;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertIntegerBool(Conversion * conversion, ChannelValue * value) {
    if (!ChannelTypeEq(ChannelValueType(value), &ChannelTypeInteger)) {
        mcx_log(LOG_ERROR, "Type conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeInteger));
        return RETURN_ERROR;
    }

    value->type = &ChannelTypeBool;
    value->value.i = (value->value.i != 0) ? 1 : 0;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertSingletonDoubleToDouble(Conversion * conversion, ChannelValue * value) {
    if (!(ChannelTypeIsArray(ChannelValueType(value)) && value->value.a.type, &ChannelTypeDouble)) {
        ChannelType * array = ChannelTypeArray(&ChannelTypeDouble, 0, NULL);
        mcx_log(LOG_ERROR, "Type conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), array);
        ChannelTypeDestructor(array);
        return RETURN_ERROR;
    }

    double val = *(double *)value->value.a.data;

    ChannelValueDataDestructor(&value->value, value->type);
    ChannelTypeDestructor(value->type);
    value->type = &ChannelTypeDouble;

    value->value.d = val;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertId(Conversion * conversion, ChannelValue * value) {
    return RETURN_OK;
}

static McxStatus TypeConversionSetup(TypeConversion * typeConversion,
                                     ChannelType * fromType,
                                     ChannelType * toType) {
    Conversion * conversion = (Conversion *) typeConversion;

    if (ChannelTypeEq(fromType, toType)) {
        conversion->convert = TypeConversionConvertId;
    } else if (ChannelTypeEq(fromType, &ChannelTypeInteger) && ChannelTypeEq(toType, &ChannelTypeDouble)) {
        conversion->convert = TypeConversionConvertIntDouble;
    } else if (ChannelTypeEq(fromType, &ChannelTypeDouble) && ChannelTypeEq(toType, &ChannelTypeInteger)) {
        conversion->convert = TypeConversionConvertDoubleInt;
    } else if (ChannelTypeEq(fromType, &ChannelTypeBool) && ChannelTypeEq(toType, &ChannelTypeDouble)) {
        conversion->convert = TypeConversionConvertBoolDouble;
    } else if (ChannelTypeEq(fromType, &ChannelTypeDouble) && ChannelTypeEq(toType, &ChannelTypeBool)) {
        conversion->convert = TypeConversionConvertDoubleBool;
    } else if (ChannelTypeEq(fromType, &ChannelTypeBool) && ChannelTypeEq(toType, &ChannelTypeInteger)) {
        conversion->convert = TypeConversionConvertBoolInteger;
    } else if (ChannelTypeEq(fromType, &ChannelTypeInteger) && ChannelTypeEq(toType, &ChannelTypeBool)) {
        conversion->convert = TypeConversionConvertIntegerBool;
    } else if (ChannelTypeIsArray(fromType) && ChannelTypeEq(fromType->ty.a.inner, &ChannelTypeDouble) && ChannelTypeEq(toType, &ChannelTypeDouble)) {
        conversion->convert = TypeConversionConvertSingletonDoubleToDouble;
    } else {
        mcx_log(LOG_ERROR, "Setup type conversion: Illegal conversion selected");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static int TypeConversionIsEmpty(TypeConversion * typeConversion) {
    Conversion * conversion = (Conversion *) typeConversion;

    return (conversion->convert == TypeConversionConvertId);
}

static void TypeConversionDestructor(TypeConversion * conversion) {

}

static TypeConversion * TypeConversionCreate(TypeConversion * typeConversion) {
    Conversion * conversion = (Conversion *) typeConversion;

    conversion->convert = TypeConversionConvertId;

    typeConversion->Setup   = TypeConversionSetup;
    typeConversion->IsEmpty = TypeConversionIsEmpty;

    return typeConversion;
}

OBJECT_CLASS(TypeConversion, Conversion);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */