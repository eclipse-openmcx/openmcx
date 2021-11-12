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
#include "core/channels/ChannelValueReference.h"
#include "core/channels/ChannelValue.h"
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

static int RangeConversionElemwiseLeq(void * first, void * second, ChannelType * type) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            return *(double *) first <= *(double *) second;
        case CHANNEL_INTEGER:
            return *(int *) first <= *(int *) second;
        default:
            return 0;
    }
}

static int RangeConversionElemwiseGeq(void * first, void * second, ChannelType * type) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            return *(double *) first >= *(double *) second;
        case CHANNEL_INTEGER:
            return *(int *) first >= *(int *) second;
        default:
            return 0;
    }
}

static McxStatus RangeConversionConvert(Conversion * conversion, ChannelValue * value) {
    RangeConversion * rangeConversion = (RangeConversion *) conversion;
    McxStatus retVal = RETURN_OK;

    if (!ChannelTypeEq(ChannelValueType(value), rangeConversion->type)) {
        mcx_log(LOG_ERROR,
                "Range conversion: Value has wrong type %s, expected: %s",
                ChannelTypeToString(ChannelValueType(value)),
                ChannelTypeToString(rangeConversion->type));
        return RETURN_ERROR;
    }

    if (rangeConversion->min) {
        retVal = ChannelValueDataSetFromReferenceIfElemwisePred(&value->value,
                                                                value->type,
                                                                &rangeConversion->min->value,
                                                                RangeConversionElemwiseLeq);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Range conversion: Set value to min failed");
            return RETURN_ERROR;
        }
    }

    if (rangeConversion->max) {
        retVal = ChannelValueDataSetFromReferenceIfElemwisePred(&value->value,
                                                                value->type,
                                                                &rangeConversion->max->value,
                                                                RangeConversionElemwiseGeq);
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

    conversion->type = ChannelTypeClone(ChannelValueType(min ? min : max));

    if (!(ChannelTypeEq(ChannelTypeBaseType(conversion->type), &ChannelTypeDouble) ||
          ChannelTypeEq(ChannelTypeBaseType(conversion->type), &ChannelTypeInteger)))
    {
        mcx_log(LOG_ERROR, "Range conversion is not defined for type %s", ChannelTypeToString(conversion->type));
        return RETURN_ERROR;
    }

    conversion->min = min ? ChannelValueClone(min) : NULL;
    conversion->max = max ? ChannelValueClone(max) : NULL;

    return RETURN_OK;
}

static int RangeConversionElementEqualsMin(void * element, ChannelType * type) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            return *(double *) element == (-DBL_MAX);
        case CHANNEL_INTEGER:
            return *(int *) element == INT_MIN;
        default:
            return 0;
    }
}

static int RangeConversionElementEqualsMax(void * element, ChannelType * type) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            return *(double *) element == DBL_MAX;
        case CHANNEL_INTEGER:
            return *(int *) element == INT_MAX;
        default:
            return 0;
    }
}

static int RangeConversionIsEmpty(RangeConversion * conversion) {
    switch (conversion->type->con) {
        case CHANNEL_DOUBLE:
            return (!conversion->min || *(double *) ChannelValueReference(conversion->min) == (-DBL_MAX)) &&
                   (!conversion->max || *(double *) ChannelValueReference(conversion->max) == DBL_MAX);
        case CHANNEL_INTEGER:
            return (!conversion->min || *(int *) ChannelValueReference(conversion->min) == INT_MIN) &&
                   (!conversion->max || *(int *) ChannelValueReference(conversion->max) == INT_MAX);
        case CHANNEL_ARRAY:
            return (!conversion->min || mcx_array_all(&conversion->min->value.a, RangeConversionElementEqualsMin)) &&
                   (!conversion->max || mcx_array_all(&conversion->max->value.a, RangeConversionElementEqualsMax));
        default:
            return 1;
    }
}

static void RangeConversionDestructor(RangeConversion * rangeConversion) {
    if (rangeConversion->type) {
        ChannelTypeDestructor(rangeConversion->type);
    }

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
static double UnitConversionConvertValue(UnitConversion * conversion, double value) {
    value = (value + conversion->source.offset) * conversion->source.factor;
    value = (value / conversion->target.factor) - conversion->target.offset;

    return value;
}

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
            vector[i] = UnitConversionConvertValue(unitConversion, vector[i]);
        }
    }
}

static McxStatus UnitConversionConvert(Conversion * conversion, ChannelValue * value) {
    UnitConversion * unitConversion = (UnitConversion *) conversion;

    if (!ChannelTypeEq(ChannelTypeBaseType(ChannelValueType(value)), &ChannelTypeDouble)) {
        mcx_log(LOG_ERROR, "Unit conversion: Value has wrong type %s, expected: %s", ChannelTypeToString(ChannelValueType(value)), ChannelTypeToString(&ChannelTypeDouble));
        return RETURN_ERROR;
    }

    if (ChannelTypeIsArray(value->type)) {
        size_t i = 0;

        for (i = 0; i < mcx_array_num_elements(&value->value.a); i++) {
            double * elem = (double *)mcx_array_get_elem_reference(&value->value.a, i);
            if (!elem) {
                return RETURN_ERROR;
            }

            *elem = UnitConversionConvertValue(conversion, *elem);
        }
    } else {
        double val = UnitConversionConvertValue(unitConversion, *(double *) ChannelValueReference(value));
        if (RETURN_OK != ChannelValueSetFromReference(value, &val)) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus UnitConversionValueRefConversion(void * element, size_t idx, ChannelType * type, void * ctx) {
    double * elem = (double *) element;
    UnitConversion * conversion = (UnitConversion *) ctx;

    *elem = UnitConversionConvertValue(conversion, *elem);

    return RETURN_OK;
}

static McxStatus UnitConversionConvertValueRef(UnitConversion * conversion, ChannelValueRef * ref) {
    if (!ChannelTypeEq(ChannelTypeBaseType(ChannelValueRefGetType(ref)), &ChannelTypeDouble)) {
        mcx_log(LOG_ERROR,
                "Unit conversion: Value has wrong type %s, expected: %s",
                ChannelTypeToString(ChannelTypeBaseType(ChannelValueRefGetType(ref))),
                ChannelTypeToString(&ChannelTypeDouble));
        return RETURN_ERROR;
    }

    return ChannelValueRefElemMap(ref, UnitConversionValueRefConversion, conversion);
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
    unitConversion->ConvertValueReference = UnitConversionConvertValueRef;

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
        mcx_log(LOG_WARNING,
                "Linear conversion: Types of factor value (%s) and offset value (%s) do not match",
                ChannelTypeToString(ChannelValueType(factor)),
                ChannelTypeToString(ChannelValueType(offset)));
        return RETURN_ERROR;
    }

    conversion->type = ChannelTypeClone(ChannelValueType(factor ? factor : offset));

    if (!(ChannelTypeEq(ChannelTypeBaseType(conversion->type), &ChannelTypeDouble) ||
          ChannelTypeEq(ChannelTypeBaseType(conversion->type), &ChannelTypeInteger)))
    {
        mcx_log(LOG_WARNING, "Linear conversion is not defined for type %s", ChannelTypeToString(conversion->type));
        return RETURN_ERROR;
    }

    conversion->factor = factor ? ChannelValueClone(factor) : NULL;
    conversion->offset = offset ? ChannelValueClone(offset) : NULL;

    return RETURN_OK;
}

static int LinearConversionElementEqualsOne(void* element, ChannelType* type) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            return *(double *) element == 1.0;
        case CHANNEL_INTEGER:
            return *(int *) element == 1;
        default:
            return 0;
    }
}

static int LinearConversionElementEqualsZero(void * element, ChannelType * type) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            return *(double *) element == 0.0;
        case CHANNEL_INTEGER:
            return *(int *) element == 0;
        default:
            return 0;
    }
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
    case CHANNEL_ARRAY:
        return (!conversion->factor || mcx_array_all(&conversion->factor->value.a, LinearConversionElementEqualsOne)) &&
               (!conversion->offset || mcx_array_all(&conversion->offset->value.a, LinearConversionElementEqualsZero));
    default:
        return 1;
    }
}

static void LinearConversionDestructor(LinearConversion * linearConversion) {
    if (linearConversion->type) {
        ChannelTypeDestructor(linearConversion->type);
    }

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

static McxStatus CheckTypesValidForConversion(ChannelType * destType,
                                              ChannelType * expectedDestType)
{
    if (!ChannelTypeEq(destType, expectedDestType)) {
        mcx_log(LOG_ERROR,
                "Type conversion: Destination value has wrong type %s, expected: %s",
                ChannelTypeToString(destType),
                ChannelTypeToString(expectedDestType));
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus CheckArrayReferencingOnlyOneElement(ChannelValueRef * dest, ChannelType * expectedDestType) {
    if (!ChannelTypeIsArray(ChannelValueRefGetType(dest))) {
        mcx_log(LOG_ERROR, "Type conversion: Destination value is not an array");
        return RETURN_ERROR;
    }

    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelTypeBaseType(ChannelValueRefGetType(dest)), expectedDestType)) {
        return RETURN_ERROR;
    }

    // check only one element referenced
    if (dest->type == CHANNEL_VALUE_REF_VALUE) {
        if (ChannelTypeNumElements(dest->ref.value->type) != 1) {
            mcx_log(LOG_ERROR, "Type conversion: Destination value is not an array of size 1");
            return RETURN_ERROR;
        }
    } else {
        if (ChannelDimensionNumElements(dest->ref.slice->dimension) != 1) {
            mcx_log(LOG_ERROR, "Type conversion: Destination value is not an array of size 1");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus TypeConversionConvertIntDouble(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelValueRefGetType(dest), &ChannelTypeDouble)) {
        return RETURN_ERROR;
    }

    dest->ref.value->value.d = (double) *((int *) src);

    return RETURN_OK;
}

static McxStatus TypeConversionConvertDoubleInt(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelValueRefGetType(dest), &ChannelTypeInteger)) {
        return RETURN_ERROR;
    }

    dest->ref.value->value.i = (int) floor(*((double *) src) + 0.5);

    return RETURN_OK;
}

static McxStatus TypeConversionConvertBoolDouble(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelValueRefGetType(dest), &ChannelTypeDouble)) {
        return RETURN_ERROR;
    }

    dest->ref.value->value.d = *((int *) src) != 0 ? 1. : 0.;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertDoubleBool(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelValueRefGetType(dest), &ChannelTypeBool)) {
        return RETURN_ERROR;
    }

    dest->ref.value->value.i = *((double *) src) > 0 ? 1 : 0;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertBoolInteger(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelValueRefGetType(dest), &ChannelTypeInteger)) {
        return RETURN_ERROR;
    }

    dest->ref.value->value.i = *((int *) src) != 0 ? 1 : 0;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertIntegerBool(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelValueRefGetType(dest), &ChannelTypeBool)) {
        return RETURN_ERROR;
    }

    dest->ref.value->value.i = *((int *) src) != 0 ? 1 : 0;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertSingletonDoubleToDouble(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckTypesValidForConversion(ChannelValueRefGetType(dest), &ChannelTypeDouble)) {
        return RETURN_ERROR;
    }

    dest->ref.value->value.d = *(double *) ((mcx_array*)src)->data;

    return RETURN_OK;
}

static McxStatus TypeConversionConvertDoubleToArrayDouble(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckArrayReferencingOnlyOneElement(dest, &ChannelTypeDouble)) {
        return RETURN_ERROR;
    }

    if (dest->type == CHANNEL_VALUE_REF_VALUE) {
        *(double *) dest->ref.value->value.a.data = *(double *) src;
    } else {
        double * data = dest->ref.slice->ref->value.a.data;
        *(data + dest->ref.slice->dimension->startIdxs[0]) = *(double *) src;
    }

    return RETURN_OK;
}

static McxStatus TypeConversionConvertIntegerToArrayDouble(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckArrayReferencingOnlyOneElement(dest, &ChannelTypeDouble)) {
        return RETURN_ERROR;
    }

    if (dest->type == CHANNEL_VALUE_REF_VALUE) {
        *(double *) dest->ref.value->value.a.data = (double) *((int *) src);
    } else {
        double * data = dest->ref.slice->ref->value.a.data;
        *(data + dest->ref.slice->dimension->startIdxs[0]) = (double) *((int *) src);
    }

    return RETURN_OK;
}

static McxStatus TypeConversionConvertBoolToArrayDouble(Conversion * conversion, ChannelValueRef * dest, void * src) {
    if (RETURN_ERROR == CheckArrayReferencingOnlyOneElement(dest, &ChannelTypeDouble)) {
        return RETURN_ERROR;
    }

    if (dest->type == CHANNEL_VALUE_REF_VALUE) {
        *(double *) dest->ref.value->value.a.data = *((int *) src) != 0 ? 1. : 0.;
    } else {
        double * data = dest->ref.slice->ref->value.a.data;
        *(data + dest->ref.slice->dimension->startIdxs[0]) = *((int *) src) != 0 ? 1. : 0.;
    }

    return RETURN_OK;
}

static McxStatus TypeConversionConvertId(Conversion * conversion, ChannelValueRef * dest, void * src) {
    return ChannelValueRefSetFromReference(dest, src, NULL, NULL);
}

static McxStatus TypeConversionSetup(TypeConversion * conversion,
                                     ChannelType * fromType,
                                     ChannelType * toType) {

    if (ChannelTypeEq(fromType, toType)) {
        conversion->Convert = TypeConversionConvertId;
    /* scalar <-> scalar */
    } else if (ChannelTypeEq(fromType, &ChannelTypeInteger) && ChannelTypeEq(toType, &ChannelTypeDouble)) {
        conversion->Convert = TypeConversionConvertIntDouble;
    } else if (ChannelTypeEq(fromType, &ChannelTypeDouble) && ChannelTypeEq(toType, &ChannelTypeInteger)) {
        conversion->Convert = TypeConversionConvertDoubleInt;
    } else if (ChannelTypeEq(fromType, &ChannelTypeBool) && ChannelTypeEq(toType, &ChannelTypeDouble)) {
        conversion->Convert = TypeConversionConvertBoolDouble;
    } else if (ChannelTypeEq(fromType, &ChannelTypeDouble) && ChannelTypeEq(toType, &ChannelTypeBool)) {
        conversion->Convert = TypeConversionConvertDoubleBool;
    } else if (ChannelTypeEq(fromType, &ChannelTypeBool) && ChannelTypeEq(toType, &ChannelTypeInteger)) {
        conversion->Convert = TypeConversionConvertBoolInteger;
    } else if (ChannelTypeEq(fromType, &ChannelTypeInteger) && ChannelTypeEq(toType, &ChannelTypeBool)) {
        conversion->Convert = TypeConversionConvertIntegerBool;
    /* scalar <-> array */
    } else if (ChannelTypeIsArray(fromType) && ChannelTypeEq(fromType->ty.a.inner, &ChannelTypeDouble) && ChannelTypeEq(toType, &ChannelTypeDouble)) {
        conversion->Convert = TypeConversionConvertSingletonDoubleToDouble;
    } else if (ChannelTypeEq(fromType, &ChannelTypeDouble) && ChannelTypeIsArray(toType) && ChannelTypeEq(toType->ty.a.inner, &ChannelTypeDouble)) {
        conversion->Convert = TypeConversionConvertDoubleToArrayDouble;
    } else if (ChannelTypeEq(fromType, &ChannelTypeInteger) && ChannelTypeIsArray(toType) && ChannelTypeEq(toType->ty.a.inner, &ChannelTypeDouble)) {
        conversion->Convert = TypeConversionConvertIntegerToArrayDouble;
    } else if (ChannelTypeEq(fromType, &ChannelTypeBool) && ChannelTypeIsArray(toType) && ChannelTypeEq(toType->ty.a.inner, &ChannelTypeDouble)) {
        conversion->Convert = TypeConversionConvertBoolToArrayDouble;
    } else {
        mcx_log(LOG_ERROR, "Setup type conversion: Illegal conversion selected");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void TypeConversionDestructor(TypeConversion * conversion) {

}

static TypeConversion * TypeConversionCreate(TypeConversion * conversion) {
    conversion->Setup = TypeConversionSetup;
    conversion->Convert = NULL;

    return conversion;
}

OBJECT_CLASS(TypeConversion, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */