/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/channels/ChannelValue.h"
#include "util/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

ChannelType ChannelTypeUnknown = { CHANNEL_UNKNOWN, NULL};
ChannelType ChannelTypeInteger = { CHANNEL_INTEGER, NULL};
ChannelType ChannelTypeDouble = { CHANNEL_DOUBLE, NULL};
ChannelType ChannelTypeBool = { CHANNEL_BOOL, NULL};
ChannelType ChannelTypeString = { CHANNEL_STRING, NULL};
ChannelType ChannelTypeBinary = { CHANNEL_BINARY, NULL};
ChannelType ChannelTypeBinaryReference = { CHANNEL_BINARY_REFERENCE, NULL};


char * CreateIndexedName(const char * name, unsigned i) {
    size_t len = 0;
    char * buffer = NULL;

    len = strlen(name) + (mcx_digits10(i) + 1) + 2 + 1;

    buffer = (char *) mcx_calloc(len, sizeof(char));
    if (!buffer) {
        return NULL;
    }

    snprintf(buffer, len, "%s[%d]", name, i);

    return buffer;
}


ChannelType * ChannelTypeClone(ChannelType * type) {
    switch (type->con) {
    case CHANNEL_UNKNOWN:
    case CHANNEL_INTEGER:
    case CHANNEL_DOUBLE:
    case CHANNEL_BOOL:
    case CHANNEL_STRING:
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
        // Scalar types are used statically (&ChannelTypeDouble, etc)
        return type;
    case CHANNEL_ARRAY: {
        ChannelType * clone = (ChannelType *) mcx_calloc(sizeof(ChannelType), 1);
        if (!clone) { return NULL; }

        clone->con = type->con;

        clone->ty.a.inner = ChannelTypeClone(type->ty.a.inner);
        clone->ty.a.numDims = type->ty.a.numDims;
        clone->ty.a.dims = mcx_copy(type->ty.a.dims, sizeof(size_t) * type->ty.a.numDims);
        if (!clone->ty.a.dims) {
            mcx_free(clone);
            return NULL;
        }

        return clone;
    }
    }

    return NULL;
}

void ChannelTypeDestructor(ChannelType * type) {
    if (&ChannelTypeUnknown == type) { }
    else if (&ChannelTypeInteger == type) { }
    else if (&ChannelTypeDouble == type) { }
    else if (&ChannelTypeBool == type) { }
    else if (&ChannelTypeString == type) { }
    else if (&ChannelTypeBinary == type) { }
    else if (&ChannelTypeBinaryReference == type) { }
    else if (type->con == CHANNEL_ARRAY) {
        // other ChannelTypes are static
        ChannelTypeDestructor(type->ty.a.inner);
        mcx_free(type->ty.a.dims);
        mcx_free(type);
    } else {
        mcx_free(type);
    }
}

ChannelType * ChannelTypeArray(ChannelType * inner, size_t numDims, size_t * dims) {
    ChannelType * array = NULL;

    if (!inner) {
        return &ChannelTypeUnknown;
    }

    array = (ChannelType *) mcx_malloc(sizeof(ChannelType));
    if (!array) {
        return &ChannelTypeUnknown;
    }

    array->con = CHANNEL_ARRAY;
    array->ty.a.inner = inner;
    array->ty.a.numDims = numDims;
    array->ty.a.dims = (size_t *) mcx_calloc(sizeof(size_t), numDims);
    if (!array->ty.a.dims) {
        return &ChannelTypeUnknown;
    }

    memcpy(array->ty.a.dims, dims, sizeof(size_t)*numDims);

    return array;
}

ChannelType * ChannelTypeArrayInner(ChannelType * array) {
    if (!ChannelTypeIsArray(array)) {
        return &ChannelTypeUnknown;
    }

    return array->ty.a.inner;
}

int ChannelTypeIsValid(ChannelType * a) {
    return a->con != CHANNEL_UNKNOWN;
}

int ChannelTypeIsScalar(ChannelType * a) {
    return a->con != CHANNEL_ARRAY;
}

int ChannelTypeIsArray(ChannelType * a) {
    return a->con == CHANNEL_ARRAY;
}

int ChannelTypeIsBinary(ChannelType * a) {
    return a->con == CHANNEL_BINARY || a->con == CHANNEL_BINARY_REFERENCE;
}

ChannelType * ChannelTypeBaseType(ChannelType * a) {
    if (ChannelTypeIsArray(a)) {
        return ChannelTypeBaseType(a->ty.a.inner);
    } else {
        return a;
    }
}

int ChannelTypeEq(ChannelType * a, ChannelType * b) {
    if (a->con == CHANNEL_ARRAY && b->con == CHANNEL_ARRAY) {
        size_t i = 0;
        if (a->ty.a.numDims != b->ty.a.numDims) {
            return 0;
        }
        for (i = 0; i < a->ty.a.numDims; i++) {
            if (a->ty.a.dims[i] != b->ty.a.dims[i]) {
                return 0;
            }
        }
        return 1;
    } else if ((a->con == CHANNEL_BINARY || a->con == CHANNEL_BINARY_REFERENCE) &&
        (b->con == CHANNEL_BINARY || b->con == CHANNEL_BINARY_REFERENCE)) {
    } else {
        return a->con == b->con;
    }
}

McxStatus mcx_array_init(mcx_array * a, size_t numDims, size_t * dims, ChannelType * inner) {
    a->numDims = numDims;
    a->dims = (size_t *) mcx_calloc(sizeof(size_t), numDims);
    if (!a->dims) {
        return RETURN_ERROR;
    }
    memcpy(a->dims, dims, sizeof(size_t) * numDims);

    a->type = inner;
    a->data = (void *) mcx_calloc(ChannelValueTypeSize(inner), mcx_array_num_elements(a));
    if (!a->data) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

void mcx_array_destroy(mcx_array * a) {
    if (a->dims) { mcx_free(a->dims); }
    if (a->data) { mcx_free(a->data); }
    if (a->type) { ChannelTypeDestructor(a->type); }
}

int mcx_array_dims_match(mcx_array * a, mcx_array * b) {
    size_t i = 0;

    if (a->numDims != b->numDims) {
        return 0;
    }
    if (a->dims == NULL || b->dims == NULL) {
        return 0;
    }

    for (i = 0; i < a->numDims; i++) {
        if (a->dims[i] != b->dims[i]) {
            return 0;
        }
    }

    return 1;
}

size_t mcx_array_num_elements(mcx_array * a) {
    size_t i = 0;
    size_t n = 1;

    if (a->numDims == 0) {
        return 0;
    }

    for (i = 0; i < a->numDims; i++) {
        n *= a->dims[i];
    }

    return n;
}

void ChannelValueInit(ChannelValue * value, ChannelType * type) {
    value->type = type;
    ChannelValueDataInit(&value->value, type);
}

void ChannelValueDataDestructor(ChannelValueData * data, ChannelType * type) {
    if (type->con == CHANNEL_STRING) {
        if (data->s) {
            mcx_free(data->s);
            data->s = NULL;
        }
    } else if (type->con == CHANNEL_BINARY) {
        if (data->b.data) {
            mcx_free(data->b.data);
            data->b.data = NULL;
        }
    } else if (type->con == CHANNEL_BINARY_REFERENCE) {
        // do not free references to binary, they are not owned by the ChannelValueData
    } else if (type->con == CHANNEL_ARRAY) {
        if (data->a.dims) {

            mcx_free(data->a.dims);
            data->a.dims = NULL;
        }
        if (data->a.data) {
            mcx_free(data->a.data);
            data->a.data = NULL;
        }
    }
}

void ChannelValueDestructor(ChannelValue * value) {
    ChannelValueDataDestructor(&value->value, value->type);
    ChannelTypeDestructor(value->type);
}

static int isSpecialChar(unsigned char c) {
    // don't allow control characters
    return (c < ' ' || c > '~');
}

size_t ChannelValueDataDoubleToBuffer(char * buffer, void * value, size_t i) {
    const size_t precision = 13;
    return sprintf(buffer, "%*.*E", (unsigned) precision, (unsigned) precision, ((double *) value)[i]);
}

size_t ChannelValueDataIntegerToBuffer(char * buffer, void * value, size_t i) {
    return sprintf(buffer, "%d", ((int *) value)[i]);
}

size_t ChannelValueDataBoolToBuffer(char * buffer, void * value, size_t i) {
    return sprintf(buffer, "%1d", (((int *) value)[i] != 0) ? 1 : 0);
}

char * ChannelValueToString(ChannelValue * value) {
    size_t i = 0;
    size_t length = 0;
    const size_t precision = 13;
    const uint32_t digits_of_exp = 4; // = (mcx_digits10(DBL_MAX_10_EXP) + 1 /* sign */
    char * buffer = NULL;

    switch (value->type->con) {
    case CHANNEL_DOUBLE:
        length = 1 /* sign */ + 1 /* pre decimal place */ + 1 /* dot */ + precision + digits_of_exp + 1 /* string termination */;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        ChannelValueDataDoubleToBuffer(buffer, &value->value.d, 0);
        break;
    case CHANNEL_INTEGER:
        length = 1 /* sign */ + mcx_digits10(abs(value->value.i)) + 1 /* string termination*/;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        ChannelValueDataIntegerToBuffer(buffer, &value->value.i, 0);
        break;
    case CHANNEL_BOOL:
        length = 2;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        ChannelValueDataBoolToBuffer(buffer, &value->value.i, 0);
        break;
    case CHANNEL_STRING:
        if (!value->value.s) {
            return NULL;
        }
        length = strlen(value->value.s) + 1 /* string termination */;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        sprintf(buffer, "%s", value->value.s);
        // remove all non printable characters and quotation marks
        if (length > 0) {
            for (i = 0; i < length - 1; i++) {
                if (isSpecialChar(buffer[i])) {
                    buffer[i] = '_';
                }
            }
        }
        break;
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
        length = value->value.b.len * 4 + 1;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        {
            size_t i = 0;

            for (i = 0; i < value->value.b.len; i++) {
                sprintf(buffer + (4 * i), "\\x%02x", value->value.b.data[i]);
            }
        }
        break;
    case CHANNEL_ARRAY:{
        size_t i = 0;
        size_t n = 0;

        size_t (*fmt)(char * buffer, void * value, size_t i);

        if (ChannelTypeEq(ChannelTypeArrayInner(value->type), &ChannelTypeDouble)) {
            fmt = ChannelValueDataDoubleToBuffer;
        } else if (ChannelTypeEq(ChannelTypeArrayInner(value->type), &ChannelTypeInteger)) {
            fmt = ChannelValueDataIntegerToBuffer;
        } else if (ChannelTypeEq(ChannelTypeArrayInner(value->type), &ChannelTypeBool)) {
            fmt = ChannelValueDataBoolToBuffer;
        } else {
            return NULL;
        }

        length = 1 /* sign */ + 1 /* pre decimal place */ + 1 /* dot */ + precision + digits_of_exp + 1 /* string termination */;
        length *= mcx_array_num_elements(&value->value.a);
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }

        if (mcx_array_num_elements(&value->value.a) > 0) {
            n += fmt(buffer + n, value->value.a.data, 0);
            for (i = 1; i < mcx_array_num_elements(&value->value.a); i++) {
                n += sprintf(buffer + n, ",");
                n += fmt(buffer + n, value->value.a.data, i);
            }
        }

        break;
    }
    default:
        return NULL;
    }

    return buffer;
}

McxStatus ChannelValueDataToStringBuffer(const ChannelValueData * value, ChannelType * type, char * buffer, size_t len) {
    size_t i = 0;
    size_t length = 0;
    const size_t precision = 13;
    const uint32_t digits_of_exp = 4; // = (mcx_digits10(DBL_MAX_10_EXP) + 1 /* sign */
    const char * doubleFmt = "%*.*E";

    switch (type->con) {
    case CHANNEL_DOUBLE:
        length = 1 /* sign */ + 1 /* pre decimal place */ + 1 /* dot */ + precision + digits_of_exp + 1 /* string termination */;
        if (len < length) {
            mcx_log(LOG_ERROR, "Port value to string: buffer too short. Needed: %d, given: %d", length, len);
            return RETURN_ERROR;
        }
        sprintf(buffer, doubleFmt, (unsigned)precision, (unsigned)precision, value->d);
        break;
    case CHANNEL_INTEGER:
        length = 1 /* sign */ + mcx_digits10(abs(value->i)) + 1 /* string termination*/;

        if (len < length) {
            mcx_log(LOG_ERROR, "Port value to string: buffer too short. Needed: %d, given: %d", length, len);
            return RETURN_ERROR;
        }
        sprintf(buffer, "%d", value->i);
        break;
    case CHANNEL_BOOL:
        length = 2;
        if (len < length) {
            mcx_log(LOG_ERROR, "Port value to string: buffer too short. Needed: %d, given: %d", length, len);
            return RETURN_ERROR;
        }
        sprintf(buffer, "%1d", (value->i != 0) ? 1 : 0);
        break;
    case CHANNEL_STRING:
        if (!value->s) {
            mcx_log(LOG_ERROR, "Port value to string: value empty");
            return RETURN_ERROR;
        }
        length = strlen(value->s) + 1 /* string termination */;
        if (len < length) {
            mcx_log(LOG_ERROR, "Port value to string: buffer too short. Needed: %d, given: %d", length, len);
            return RETURN_ERROR;
        }
        sprintf(buffer, "%s", value->s);
        // remove all non printable characters and quotation marks
        for (i = 1; i < length - 2; i++) {
            if (isSpecialChar(buffer[i])) {
                buffer[i] = '_';
            }
        }
        break;
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
        length = value->b.len * 4 + 1;
        if (len < length) {
            mcx_log(LOG_DEBUG, "Port value to string: buffer too short. Needed: %d, given: %d", length, len);
            return RETURN_ERROR;
        }
        {
            size_t i = 0;

            for (i = 0; i < value->b.len; i++) {
                sprintf(buffer + (4 * i), "\\x%02x", value->b.data[i]);
            }
        }
        break;
    case CHANNEL_ARRAY: {
    const char * doubleFmt = "% *.*E";

        length = 1 /* sign */ + 1 /* pre decimal place */ + 1 /* dot */ + precision + digits_of_exp + 1 /* string termination */;
        if (len < length) {
            mcx_log(LOG_ERROR, "Port value to string: buffer too short. Needed: %d, given: %d", length, len);
            return RETURN_ERROR;
        }
        sprintf(buffer, doubleFmt, (unsigned)precision, (unsigned)precision, *(double *)value->a.data);


        break;
    }
    default:
        mcx_log(LOG_DEBUG, "Port value to string: Unknown type");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus ChannelValueToStringBuffer(const ChannelValue * value, char * buffer, size_t len) {
    return ChannelValueDataToStringBuffer(&value->value, value->type, buffer, len);
}

ChannelType * ChannelValueType(ChannelValue * value) {
    return value->type;
}

void * ChannelValueReference(ChannelValue * value) {
    if (value->type->con == CHANNEL_UNKNOWN) {
        return NULL;
    } else {
        return &value->value;
    }
}

void ChannelValueDataInit(ChannelValueData * data, ChannelType * type) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            data->d = 0.0;
            break;
        case CHANNEL_INTEGER:
            data->i = 0;
            break;
        case CHANNEL_BOOL:
            data->i = 0;
            break;
        case CHANNEL_STRING:
            data->s = NULL;
            break;
        case CHANNEL_BINARY:
        case CHANNEL_BINARY_REFERENCE:
            data->b.len = 0;
            data->b.data = NULL;
            break;
        case CHANNEL_ARRAY: {
            void * tmp = data->a.dims;
            data->a.type = ChannelTypeClone(type->ty.a.inner);
            data->a.numDims = type->ty.a.numDims;
            data->a.dims = (size_t *) mcx_calloc(sizeof(size_t), type->ty.a.numDims);
            if (data->a.dims) {
                memcpy(data->a.dims, type->ty.a.dims, type->ty.a.numDims * sizeof(size_t));
            }
            data->a.data = mcx_calloc(mcx_array_num_elements(&data->a), ChannelValueTypeSize(data->a.type));
            break;
        }
        case CHANNEL_UNKNOWN:
        default:
            break;
    }
}

McxStatus ChannelValueDataSetFromReference(ChannelValueData * data, ChannelType * type, const void * reference) {
    if (!reference) { return RETURN_OK; }

    switch (type->con) {
    case CHANNEL_DOUBLE:
        data->d = * (double *) reference;
        break;
    case CHANNEL_INTEGER:
        data->i = * (int *) reference;
        break;
    case CHANNEL_BOOL:
        data->i = * (int *) reference;
        break;
    case CHANNEL_STRING:
        if (NULL != reference && NULL != * (char * *) reference ) {
            if (data->s) {
                mcx_free(data->s);
            }
            data->s = (char *) mcx_calloc(strlen(* (char * *) reference) + 1, sizeof(char));
            if (data->s) {
                strncpy(data->s, * (char * *) reference, strlen(* (char * *) reference) + 1);
            }
        }
        break;
    case CHANNEL_BINARY:
        if (NULL != reference && NULL != ((binary_string *) reference)->data) {
            if (data->b.data) {
                mcx_free(data->b.data);
            }
            data->b.len = ((binary_string *) reference)->len;
            data->b.data = (char *) mcx_calloc(data->b.len, 1);
            if (data->b.data) {
                memcpy(data->b.data, ((binary_string *) reference)->data, data->b.len);
            }
        }
        break;
    case CHANNEL_BINARY_REFERENCE:
        if (NULL != reference) {
            data->b.len = ((binary_string *) reference)->len;
            data->b.data = ((binary_string *) reference)->data;
        }
        break;
    case CHANNEL_ARRAY:
        if (NULL != reference) {
            mcx_array * a = (mcx_array *) reference;

            // The first call to SetFromReference fixes the dimensions
            if (!data->a.numDims && a->numDims) {
                if (RETURN_OK != mcx_array_init(&data->a, a->numDims, a->dims, a->type)) {
                    return RETURN_ERROR;
                }
            }

            // Arrays do not support multiplexing (yet)
            if (!mcx_array_dims_match(&data->a, a)) {
                return RETURN_ERROR;
            }

            if (a->data == NULL || data->a.data == NULL) {
                return RETURN_ERROR;
            }

            memcpy(data->a.data, a->data, ChannelValueTypeSize(data->a.type) * mcx_array_num_elements(&data->a));
        }
    case CHANNEL_UNKNOWN:
    default:
        break;
    }

    return RETURN_OK;
}

McxStatus ChannelValueSetFromReference(ChannelValue * value, const void * reference) {
    return ChannelValueDataSetFromReference(&value->value, value->type, reference);
}

McxStatus ChannelValueSet(ChannelValue * value, const ChannelValue * source) {
    if (!ChannelTypeEq(value->type, source->type)) {
        mcx_log(LOG_ERROR, "Port: Set: Mismatching types. Source type: %s, target type: %s",
            ChannelTypeToString(source->type), ChannelTypeToString(value->type));
        return RETURN_ERROR;
    }

    if (RETURN_OK != ChannelValueSetFromReference(value, ChannelValueReference((ChannelValue *) source))) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus ChannelValueSetToReference(ChannelValue * value, void * reference) {
    switch (value->type->con) {
    case CHANNEL_DOUBLE:
        * (double *) reference = value->value.d;
        break;
    case CHANNEL_INTEGER:
        * (int *) reference = value->value.i;
        break;
    case CHANNEL_BOOL:
        * (int *) reference = value->value.i;
        break;
    case CHANNEL_STRING:
        if (* (char **) reference) {
            mcx_free(* (char **) reference);
            * (char **) reference = NULL;
        }
        if (value->value.s) {
            * (char **) reference = (char *) mcx_calloc(strlen(value->value.s) + 1, sizeof(char));
            if (* (char **) reference) {
                strncpy(* (char **) reference, value->value.s, strlen(value->value.s) + 1);
            }
        }
        break;
    case CHANNEL_BINARY:
        if (NULL != reference && NULL != ((binary_string *) reference)->data) {
            mcx_free(((binary_string *) reference)->data);
            ((binary_string *) reference)->data = NULL;
        }
        if (value->value.b.data) {
            ((binary_string *) reference)->len = value->value.b.len;
            ((binary_string *) reference)->data = (char *) mcx_calloc(value->value.b.len, 1);
            if (((binary_string *) reference)->data) {
                memcpy(((binary_string *) reference)->data, value->value.b.data, value->value.b.len);
            }
        }
        break;
    case CHANNEL_BINARY_REFERENCE:
        if (NULL != reference) {
            ((binary_string *) reference)->len = value->value.b.len;
            ((binary_string *) reference)->data = value->value.b.data;
        }
        break;
    case CHANNEL_ARRAY:
        if (NULL != reference) {
            mcx_array * a = (mcx_array *) reference;

            // First Set fixes the dimensions
            if (value->value.a.numDims && !a->numDims) {
                if (RETURN_OK != mcx_array_init(a, value->value.a.numDims, value->value.a.dims, value->value.a.type)) {
                    return RETURN_ERROR;
                }
            }

            // Arrays do not support multiplexing (yet)
            if (!mcx_array_dims_match(a, &value->value.a)) {
                return RETURN_ERROR;
            }

            if (value->value.a.data == NULL || a->data == NULL) {
                return RETURN_ERROR;
            }

            memcpy(a->data, value->value.a.data, ChannelValueTypeSize(a->type) * mcx_array_num_elements(a));
        }
        break;
    case CHANNEL_UNKNOWN:
    default:
        break;
    }

    return RETURN_OK;
}

#ifdef __cplusplus
size_t ChannelValueTypeSize(ChannelType * type) {
    switch (type->con) {
    case CHANNEL_DOUBLE:
        return sizeof(ChannelValueData::d);
    case CHANNEL_INTEGER:
        return sizeof(ChannelValueData::i);
    case CHANNEL_BOOL:
        return sizeof(ChannelValueData::i);
    case CHANNEL_STRING:
        return sizeof(ChannelValueData::s);
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
        return sizeof(ChannelValueData::b);
    case CHANNEL_ARRAY:
        return sizeof(ChannelValueData::a);
    }
    return 0;
}
#else //__cplusplus
size_t ChannelValueTypeSize(ChannelType * type) {
    ChannelValueData value;
    switch (type->con) {
    case CHANNEL_DOUBLE:
        return sizeof(value.d);
    case CHANNEL_INTEGER:
        return sizeof(value.i);
    case CHANNEL_BOOL:
        return sizeof(value.i);
    case CHANNEL_STRING:
        return sizeof(value.s);
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
        return sizeof(value.b);
    case CHANNEL_ARRAY:
        return sizeof(value.a);
    }
    return 0;
}
#endif //__cplusplus

int ChannelTypeMatch(ChannelType * a, ChannelType * b) {
    return ChannelTypeEq(a, b);
}

const char * ChannelTypeToString(ChannelType * type) {
    switch (type->con) {
    case CHANNEL_UNKNOWN:
        return "Unknown";
    case CHANNEL_DOUBLE:
        return "Double";
    case CHANNEL_INTEGER:
        return "Integer";
    case CHANNEL_BOOL:
        return "Bool";
    case CHANNEL_STRING:
        return "String";
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
        return "Binary";
    case CHANNEL_ARRAY:
        return "Array";
    default:
        return "";
    }
}

ChannelValue * ChannelValueClone(ChannelValue * value) {
    if (!value) { return NULL; }

    ChannelValue * clone = (ChannelValue *) mcx_malloc(sizeof(ChannelValue));

    if (!clone) { return NULL; }

    // ChannelTypeClone might fail, then clone will be
    // ChannelTypeUnknown and ChannelValueSet below returns an error
    ChannelValueInit(clone, ChannelTypeClone(ChannelValueType(value)));

    if (ChannelValueSet(clone, value) != RETURN_OK) {
        mcx_free(clone);
        return NULL;
    }

    return clone;
}


int ChannelValueLeq(ChannelValue * val1, ChannelValue * val2) {
    if (!ChannelTypeEq(val1->type, val2->type)) {
        return 0;
    }

    switch (ChannelValueType(val1)->con) {
    case CHANNEL_DOUBLE:
        return val1->value.d <= val2->value.d;
    case CHANNEL_INTEGER:
        return val1->value.i <= val2->value.i;
    default:
        return 0;
    }
}

int ChannelValueGeq(ChannelValue * val1, ChannelValue * val2) {
    if (!ChannelTypeEq(val1->type, val2->type)) {
        return 0;
    }

    switch (ChannelValueType(val1)->con) {
    case CHANNEL_DOUBLE:
        return val1->value.d >= val2->value.d;
    case CHANNEL_INTEGER:
        return val1->value.i >= val2->value.i;
    default:
        return 0;
    }
}

int ChannelValueEq(ChannelValue * val1, ChannelValue * val2) {
    if (!ChannelTypeEq(val1->type, val2->type)) {
        return 0;
    }

    switch (ChannelValueType(val1)->con) {
    case CHANNEL_DOUBLE:
        return val1->value.d == val2->value.d;
    case CHANNEL_BOOL:
    case CHANNEL_INTEGER:
        return val1->value.i == val2->value.i;
    case CHANNEL_STRING:
        return !strcmp(val1->value.s, val2->value.s);
    default:
        return 0;
    }
}

McxStatus ChannelValueAddOffset(ChannelValue * val, ChannelValue * offset) {
    if (!ChannelTypeEq(val->type, offset->type)) {
        mcx_log(LOG_ERROR, "Port: Add offset: Mismatching types. Value type: %s, offset type: %s",
            ChannelTypeToString(ChannelValueType(val)), ChannelTypeToString(ChannelValueType(offset)));
        return RETURN_ERROR;
    }

    switch (ChannelValueType(val)->con) {
    case CHANNEL_DOUBLE:
        val->value.d += offset->value.d;
        return RETURN_OK;
    case CHANNEL_INTEGER:
        val->value.i += offset->value.i;
        return RETURN_OK;
    default:
        mcx_log(LOG_ERROR, "Port: Add offset: Type %s not allowed", ChannelTypeToString(ChannelValueType(val)));
        return RETURN_ERROR;
    }
}

McxStatus ChannelValueScale(ChannelValue * val, ChannelValue * factor) {
    if (!ChannelTypeEq(val->type, factor->type)) {
        mcx_log(LOG_ERROR, "Port: Scale: Mismatching types. Value type: %s, factor type: %s",
            ChannelTypeToString(ChannelValueType(val)), ChannelTypeToString(ChannelValueType(factor)));
        return RETURN_ERROR;
    }

    switch (ChannelValueType(val)->con) {
    case CHANNEL_DOUBLE:
        val->value.d *= factor->value.d;
        return RETURN_OK;
    case CHANNEL_INTEGER:
        val->value.i *= factor->value.i;
        return RETURN_OK;
    default:
        mcx_log(LOG_ERROR, "Port: Scale: Type %s not allowed", ChannelTypeToString(ChannelValueType(val)));
        return RETURN_ERROR;
    }
}

// Does not take ownership of dims or data
ChannelValue * ChannelValueNewScalar(ChannelType * type, void * data) {
    ChannelValue * value = mcx_malloc(sizeof(ChannelValue));
    if (!value) {
        return NULL;
    }

    ChannelValueInit(value, ChannelTypeClone(type));
    ChannelValueSetFromReference(value, data);

    return value;
}

// Does not take ownership of dims or data
ChannelValue * ChannelValueNewArray(size_t numDims, size_t dims[], ChannelType * type, void * data) {
    ChannelValue * value = mcx_malloc(sizeof(ChannelValue));
    if (!value) {
        return NULL;
    }

    ChannelValueInit(value, ChannelTypeArray(type, numDims, dims));

    if (value->value.a.data && data) {
        memcpy(value->value.a.data, data, ChannelValueTypeSize(type) * mcx_array_num_elements(&value->value.a));
    }

    return value;
}

void ChannelValueDestroy(ChannelValue ** value) {
    ChannelValueDestructor(*value);
    mcx_free(*value);
    *value = NULL;
}

ChannelValue ** ArrayToChannelValueArray(void * values, size_t num, ChannelType * type) {
    ChannelValue ** array = NULL;

    size_t size = ChannelValueTypeSize(type);
    size_t i = 0;

    if (!values) {
        return NULL;
    }

    array = (ChannelValue **) mcx_calloc(sizeof(ChannelValue*), num);
    if (!array) { return NULL; }

    for (i = 0; i < num; i++) {
        array[i] = (ChannelValue *) mcx_malloc(sizeof(ChannelValue));

        if (!array[i]) {
            size_t j = 0;
            for (j = 0; j < i; j++) {
                mcx_free(array[j]);
            }
            mcx_free(array);
            return NULL;
        }

        ChannelValueInit(array[i], ChannelTypeClone(type));
        if (RETURN_OK != ChannelValueSetFromReference(array[i], (char *) values + i*size)) {
            return RETURN_ERROR;
        }
    }

    return array;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */