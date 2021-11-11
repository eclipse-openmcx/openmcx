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

int ChannelTypeIsValid(const ChannelType * a) {
    return a->con != CHANNEL_UNKNOWN;
}

int ChannelTypeIsScalar(const ChannelType * a) {
    return a->con != CHANNEL_ARRAY;
}

int ChannelTypeIsArray(const ChannelType * a) {
    return a->con == CHANNEL_ARRAY;
}

int ChannelTypeIsBinary(const ChannelType * a) {
    return a->con == CHANNEL_BINARY || a->con == CHANNEL_BINARY_REFERENCE;
}

ChannelType * ChannelTypeBaseType(ChannelType * a) {
    if (ChannelTypeIsArray(a)) {
        return ChannelTypeBaseType(a->ty.a.inner);
    } else {
        return a;
    }
}

size_t ChannelTypeNumElements(const ChannelType * type) {
    if (ChannelTypeIsArray(type)) {
        size_t i = 0;
        size_t num_elems = 1;

        for (i = 0; i < type->ty.a.numDims; i++) {
            num_elems *= type->ty.a.dims[i];
        }

        return num_elems;
    } else {
        return 1;
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

int mcx_array_all(mcx_array * a, mcx_array_predicate_f_ptr predicate) {
    size_t i = 0;
    ChannelValueData element = {0};

    for (i = 0; i < mcx_array_num_elements(a); i++) {
        if (RETURN_OK != mcx_array_get_elem(a, i, &element)) {
            mcx_log(LOG_WARNING, "mcx_array_all: Getting element %zu failed", i);
            return 0;
        }

        if (!predicate(&element, a->type)) {
            return 0;
        }
    }

    return 1;
}

int mcx_array_leq(const mcx_array * left, const mcx_array * right) {
    size_t numElems = 0;
    size_t i = 0;

    if (!ChannelTypeEq(left->type, right->type)) {
        return 0;
    }

    numElems = mcx_array_num_elements(left);

    for (i = 0; i < numElems; i++) {
        switch (left->type->con) {
            case CHANNEL_DOUBLE:
                if (((double *)left->data)[i] > ((double*)right->data)[i]) {
                    return 0;
                }
                break;
            case CHANNEL_INTEGER:
                if (((int *) left->data)[i] > ((int *) right->data)[i]) {
                    return 0;
                }
                break;
            default:
                return 0;
        }
    }

    return 1;
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

McxStatus mcx_array_map(mcx_array * a, mcx_array_map_f_ptr fn, void * ctx) {
    size_t num_elems = mcx_array_num_elements(a);
    size_t i = 0;

    for (i = 0; i < num_elems; i++) {
        if (fn((char *) a->data + i * ChannelValueTypeSize(a->type), i, a->type, ctx)) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus mcx_array_get_elem(mcx_array * a, size_t idx, ChannelValueData * element) {
    size_t num_elems = mcx_array_num_elements(a);

    if (idx >= num_elems) {
        mcx_log(LOG_ERROR, "mcx_array_get_elem: Array index out of range (idx: %d, num_elems: %d)", idx, num_elems);
        return RETURN_ERROR;
    }

    switch (a->type->con) {
        case CHANNEL_DOUBLE:
            ChannelValueDataSetFromReference(element, a->type, ((double*)a->data) + idx);
            break;
        case CHANNEL_INTEGER:
        case CHANNEL_BOOL:
            ChannelValueDataSetFromReference(element, a->type, ((int *) a->data) + idx);
            break;
        case CHANNEL_STRING:
            ChannelValueDataSetFromReference(element, a->type, ((char **) a->data) + idx);
            break;
        case CHANNEL_BINARY:
        case CHANNEL_BINARY_REFERENCE:
            ChannelValueDataSetFromReference(element, a->type, ((binary_string *) a->data) + idx);
            break;
        case CHANNEL_ARRAY:
            ChannelValueDataSetFromReference(element, a->type, ((mcx_array *) a->data) + idx);
            break;
        default:
            mcx_log(LOG_ERROR, "mcx_array_get_elem: Unknown array type");
            return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus mcx_array_set_elem(mcx_array * a, size_t idx, ChannelValueData * element) {
    size_t num_elems = mcx_array_num_elements(a);

    if (idx >= num_elems) {
        mcx_log(LOG_ERROR, "mcx_array_set_elem: Array index out of range (idx: %d, num_elems: %d)", idx, num_elems);
        return RETURN_ERROR;
    }

    switch (a->type->con) {
        case CHANNEL_DOUBLE:
            *((double *) a->data + idx) = element->d;
            break;
        case CHANNEL_INTEGER:
            *((int *) a->data + idx) = element->i;
            break;
        case CHANNEL_BOOL:
            *((int *) a->data + idx) = element->i != 0 ? 1 : 0;
            break;
        case CHANNEL_STRING:
            {
                char ** elements = (char **) a->data;

                if (elements[idx]) {
                    mcx_free(elements[idx]);
                }

                elements[idx] = (char *) mcx_calloc(strlen(element->s) + 1, sizeof(char));
                if (!elements[idx]) {
                    mcx_log(LOG_ERROR, "mcx_array_set_elem: Not enough memory");
                    return RETURN_ERROR;
                }

                strncpy(elements[idx], element->s, strlen(element->s) + 1);
                break;
            }
        case CHANNEL_BINARY:
            {
                binary_string * elements = (binary_string *) a->data;
                if (elements[idx].data) {
                    mcx_free(elements[idx].data);
                }

                elements[idx].len = element->b.len;
                elements[idx].data = (char *) mcx_calloc(elements[idx].len, 1);
                if (!elements[idx].data) {
                    mcx_log(LOG_ERROR, "mcx_array_set_elem: Not enough memory");
                    return RETURN_ERROR;
                }
                memcpy(elements[idx].data, element->b.data, elements[idx].len);
                break;
            }
        case CHANNEL_BINARY_REFERENCE:
            ((binary_string *) a->data)[idx].len = element->b.len;
            ((binary_string *) a->data)[idx].data = element->b.data;
            break;
        case CHANNEL_ARRAY:
            mcx_log(LOG_ERROR, "mcx_array_set_elem: Nested arrays are not supported");
            return RETURN_ERROR;
        default:
            mcx_log(LOG_ERROR, "mcx_array_set_elem: Unknown array type");
            return RETURN_ERROR;
    }

    return RETURN_OK;
}

void * mcx_array_get_elem_reference(mcx_array * a, size_t idx) {
    size_t num_elems = mcx_array_num_elements(a);
    char * data = (char *) a->data;

    if (idx >= num_elems) {
        mcx_log(LOG_ERROR, "mcx_array_get_elem_reference: Array index out of range (idx: %d, num_elems: %d)", idx, num_elems);
        return RETURN_ERROR;
    }

    return data + idx * ChannelValueTypeSize(a->type);
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
                // specifier: x (hex integer) -----+
                // length: 2 (unsigned char) -----+|
                // width: 2 --------------------+ ||
                // flag: 0-padded -------------+| ||
                //                             || ||
                // string literal "\x" ------+ || ||
                sprintf(buffer + (4 * i), "\\x%02hhx", value->value.b.data[i]);
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

McxStatus ChannelValueDataSetFromReferenceIfElemwisePred(ChannelValueData * data,
                                                         ChannelType * type,
                                                         const void * reference,
                                                         fChannelValueDataSetterPredicate predicate) {
    if (!reference) {
        return RETURN_OK;
    }

    if (ChannelTypeIsArray(type)) {
        mcx_array * a = (mcx_array *) reference;
        size_t i = 0;
        ChannelValueData first = {0};
        ChannelValueData second = {0};

        if (!mcx_array_dims_match(&data->a, a)) {
            mcx_log(LOG_ERROR, "ChannelValueDataSetFromReferenceIfElemwisePred: Mismatching array dimensions");
            return RETURN_ERROR;
        }

        if (a->data == NULL || data->a.data == NULL) {
            mcx_log(LOG_ERROR, "ChannelValueDataSetFromReferenceIfElemwisePred: Array data not initialized");
            return RETURN_ERROR;
        }

        for (i = 0; i < mcx_array_num_elements(&data->a); i++) {
            if (RETURN_OK != mcx_array_get_elem(&data->a, i, &first)) {
                mcx_log(LOG_ERROR, "ChannelValueDataSetFromReferenceIfElemwisePred: Getting destination element %zu failed", i);
                return RETURN_ERROR;
            }

            if (RETURN_OK != mcx_array_get_elem(a, i, &second)) {
                mcx_log(LOG_ERROR, "ChannelValueDataSetFromReferenceIfElemwisePred: Getting source element %zu failed", i);
                return RETURN_ERROR;
            }

            if (predicate(&first, &second, a->type)) {
                mcx_array_set_elem(&data->a, i, &second);
            }
        }

        return RETURN_OK;
    } else {

    }
    switch (type->con) {
        default:
            if (predicate(data, reference, type)) {
                return ChannelValueDataSetFromReference(data, type, reference);
            }
            break;
    }

    return RETURN_OK;
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

McxStatus ChannelValueDataSetToReference(ChannelValueData * value, ChannelType * type, void * reference) {
    switch (type->con) {
        case CHANNEL_DOUBLE:
            *(double *) reference = value->d;
            break;
        case CHANNEL_INTEGER:
            *(int *) reference = value->i;
            break;
        case CHANNEL_BOOL:
            *(int *) reference = value->i;
            break;
        case CHANNEL_STRING:
            if (*(char **) reference) {
                mcx_free(*(char **) reference);
                *(char **) reference = NULL;
            }
            if (value->s) {
                *(char **) reference = (char *) mcx_calloc(strlen(value->s) + 1, sizeof(char));
                if (*(char **) reference) {
                    strncpy(*(char **) reference, value->s, strlen(value->s) + 1);
                }
            }
            break;
        case CHANNEL_BINARY:
            if (NULL != reference && NULL != ((binary_string *) reference)->data) {
                mcx_free(((binary_string *) reference)->data);
                ((binary_string *) reference)->data = NULL;
            }
            if (value->b.data) {
                ((binary_string *) reference)->len = value->b.len;
                ((binary_string *) reference)->data = (char *) mcx_calloc(value->b.len, 1);
                if (((binary_string *) reference)->data) {
                    memcpy(((binary_string *) reference)->data, value->b.data, value->b.len);
                }
            }
            break;
        case CHANNEL_BINARY_REFERENCE:
            if (NULL != reference) {
                ((binary_string *) reference)->len = value->b.len;
                ((binary_string *) reference)->data = value->b.data;
            }
            break;
        case CHANNEL_ARRAY:
            if (NULL != reference) {
                mcx_array * a = (mcx_array *) reference;

                // First Set fixes the dimensions
                if (value->a.numDims && !a->numDims) {
                    if (RETURN_OK != mcx_array_init(a, value->a.numDims, value->a.dims, value->a.type)) {
                        return RETURN_ERROR;
                    }
                }

                // Arrays do not support multiplexing (yet)
                if (!mcx_array_dims_match(a, &value->a)) {
                    return RETURN_ERROR;
                }

                if (value->a.data == NULL || a->data == NULL) {
                    return RETURN_ERROR;
                }

                memcpy(a->data, value->a.data, ChannelValueTypeSize(a->type) * mcx_array_num_elements(a));
            }
            break;
        case CHANNEL_UNKNOWN:
        default:
            break;
    }

    return RETURN_OK;

}

McxStatus ChannelValueSetToReference(ChannelValue * value, void * reference) {
    return ChannelValueDataSetToReference(&value->value, value->type, reference);
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
        return "Binary";
    case CHANNEL_BINARY_REFERENCE:
        return "BinaryReference";
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
    case CHANNEL_ARRAY:
        return mcx_array_leq(&val1->value.a, &val2->value.a);
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

static int ChannelValueArrayElemAddOffset(void * elem, size_t idx, ChannelType * type, void * ctx) {
    mcx_array * offset = (mcx_array *) ctx;

    switch (type->con) {
        case CHANNEL_DOUBLE:
            *(double *) elem = *(double *) elem + ((double *) offset->data)[idx];
            break;
        case CHANNEL_INTEGER:
            *(int *) elem = *(int *) elem + ((int *) offset->data)[idx];
            break;
        default:
            mcx_log(LOG_ERROR, "ChannelValueArrayElemAddOffset: Type %s not allowed", ChannelTypeToString(type));
            return 1;
    }

    return 0;
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
    case CHANNEL_ARRAY:
        return mcx_array_map(&val->value.a, ChannelValueArrayElemAddOffset, &offset->value.a);
    default:
        mcx_log(LOG_ERROR, "Port: Add offset: Type %s not allowed", ChannelTypeToString(ChannelValueType(val)));
        return RETURN_ERROR;
    }
}

static int ChannelValueArrayElemScale(void * elem, size_t idx, ChannelType * type, void * ctx) {
    mcx_array * factor = (mcx_array *) ctx;

    switch (type->con) {
        case CHANNEL_DOUBLE:
            *(double *) elem = *(double *) elem * ((double *) factor->data)[idx];
            break;
        case CHANNEL_INTEGER:
            *(int *) elem = *(int *) elem * ((int *) factor->data)[idx];
            break;
        default:
            mcx_log(LOG_ERROR, "ChannelValueArrayElemScale: Type %s not allowed", ChannelTypeToString(type));
            return 1;
    }

    return 0;
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
    case CHANNEL_ARRAY:
        return mcx_array_map(&val->value.a, ChannelValueArrayElemScale, &factor->value.a);
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