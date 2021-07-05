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

void ChannelValueInit(ChannelValue * value, ChannelType type) {
    value->type = type;
    ChannelValueDataInit(&value->value, type);
}

void ChannelValueDataDestructor(ChannelValueData * data, ChannelType type) {
    if (type == CHANNEL_STRING) {
        if (data->s) {
            mcx_free(data->s);
            data->s = NULL;
        }
    } else if (type == CHANNEL_BINARY) {
        if (data->b.data) {
            mcx_free(data->b.data);
            data->b.data = NULL;
        }
    } else if (type == CHANNEL_BINARY_REFERENCE) {
        // do not free references to binary, they are not owned by the ChannelValueData
    }
}

void ChannelValueDestructor(ChannelValue * value) {
    ChannelValueDataDestructor(&value->value, value->type);
}

static int isSpecialChar(unsigned char c) {
    // don't allow control characters
    return (c < ' ' || c > '~');
}

char * ChannelValueToString(ChannelValue * value) {
    size_t i = 0;
    size_t length = 0;
    const size_t precision = 13;
    const uint32_t digits_of_exp = 4; // = (mcx_digits10(DBL_MAX_10_EXP) + 1 /* sign */
    char * buffer = NULL;

    switch (value->type) {
    case CHANNEL_DOUBLE:
        length = 1 /* sign */ + 1 /* pre decimal place */ + 1 /* dot */ + precision + digits_of_exp + 1 /* string termination */;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        sprintf(buffer, "%*.*E", (unsigned) precision, (unsigned) precision, value->value.d);
        break;
    case CHANNEL_INTEGER:
        length = 1 /* sign */ + mcx_digits10(abs(value->value.i)) + 1 /* string termination*/;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        sprintf(buffer, "%d", value->value.i);
        break;
    case CHANNEL_BOOL:
        length = 2;
        buffer = (char *) mcx_malloc(sizeof(char) * length);
        if (!buffer) {
            return NULL;
        }
        sprintf(buffer, "%1d", (value->value.i != 0) ? 1 : 0);
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
    default:
        return NULL;
    }

    return buffer;
}

McxStatus ChannelValueDataToStringBuffer(const ChannelValueData * value, ChannelType type, char * buffer, size_t len) {
    size_t i = 0;
    size_t length = 0;
    const size_t precision = 13;
    const uint32_t digits_of_exp = 4; // = (mcx_digits10(DBL_MAX_10_EXP) + 1 /* sign */
    const char * doubleFmt = "%*.*E";

    switch (type) {
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
    default:
        mcx_log(LOG_DEBUG, "Port value to string: Unknown type");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus ChannelValueToStringBuffer(const ChannelValue * value, char * buffer, size_t len) {
    return ChannelValueDataToStringBuffer(&value->value, value->type, buffer, len);
}

ChannelType ChannelValueType(ChannelValue * value) {
    return value->type;
}

void * ChannelValueReference(ChannelValue * value) {
    if (value->type == CHANNEL_UNKNOWN) {
        return NULL;
    } else {
        return &value->value;
    }
}

void ChannelValueDataInit(ChannelValueData * data, ChannelType type) {
    switch (type) {
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
        case CHANNEL_UNKNOWN:
        default:
            break;
    }
}

McxStatus ChannelValueDataSetFromReference(ChannelValueData * data, ChannelType type, const void * reference) {
    if (!reference) { return RETURN_OK; } // TODO: change to ERROR

    switch (type) {
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
    if (value->type != source->type) {
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
    switch (value->type) {
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
    case CHANNEL_UNKNOWN:
    default:
        break;
    }

    return RETURN_OK;
}
#ifdef __cplusplus
size_t ChannelValueTypeSize(ChannelType type) {
    switch (type) {
    case CHANNEL_DOUBLE:
        return sizeof(ChannelValueData::d);
    case CHANNEL_INTEGER:
        return sizeof(ChannelValueData::i);
    case CHANNEL_BOOL:
        return sizeof(ChannelValueData::i);
    case CHANNEL_STRING:
        return sizeof(ChannelValueData::s);
    }
    return 0;
}
#else //__cplusplus
size_t ChannelValueTypeSize(ChannelType type) {
    ChannelValueData value;
    switch (type) {
    case CHANNEL_DOUBLE:
        return sizeof(value.d);
    case CHANNEL_INTEGER:
        return sizeof(value.i);
    case CHANNEL_BOOL:
        return sizeof(value.i);
    case CHANNEL_STRING:
        return sizeof(value.s);
    }
    return 0;
}
#endif //__cplusplus

const char * ChannelTypeToString(ChannelType type) {
    switch (type) {
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
    default:
        return "";
    }
}

ChannelValue * ChannelValueClone(ChannelValue * value) {
    ChannelValue * clone = (ChannelValue *) mcx_malloc(sizeof(ChannelValue));

    if (!clone) { return NULL; }

    ChannelValueInit(clone, ChannelValueType(value));

    if (ChannelValueSet(clone, value) != RETURN_OK) {
        mcx_free(clone);
        return NULL;
    }

    return clone;
}


int ChannelValueLeq(ChannelValue * val1, ChannelValue * val2) {
    if (ChannelValueType(val1) != ChannelValueType(val2)) {
        return 0;
    }

    switch (ChannelValueType(val1)) {
    case CHANNEL_DOUBLE:
        return val1->value.d <= val2->value.d;
    case CHANNEL_INTEGER:
        return val1->value.i <= val2->value.i;
    default:
        return 0;
    }
}

int ChannelValueGeq(ChannelValue * val1, ChannelValue * val2) {
    if (ChannelValueType(val1) != ChannelValueType(val2)) {
        return 0;
    }

    switch (ChannelValueType(val1)) {
    case CHANNEL_DOUBLE:
        return val1->value.d >= val2->value.d;
    case CHANNEL_INTEGER:
        return val1->value.i >= val2->value.i;
    default:
        return 0;
    }
}

int ChannelValueEq(ChannelValue * val1, ChannelValue * val2) {
    if (ChannelValueType(val1) != ChannelValueType(val2)) {
        return 0;
    }

    switch (ChannelValueType(val1)) {
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
    if (ChannelValueType(val) != ChannelValueType(offset)) {
        mcx_log(LOG_ERROR, "Port: Add offset: Mismatching types. Value type: %s, offset type: %s",
            ChannelTypeToString(ChannelValueType(val)), ChannelTypeToString(ChannelValueType(offset)));
        return RETURN_ERROR;
    }

    switch (ChannelValueType(val)) {
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
    if (ChannelValueType(val) != ChannelValueType(factor)) {
        mcx_log(LOG_ERROR, "Port: Scale: Mismatching types. Value type: %s, factor type: %s",
            ChannelTypeToString(ChannelValueType(val)), ChannelTypeToString(ChannelValueType(factor)));
        return RETURN_ERROR;
    }

    switch (ChannelValueType(val)) {
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

ChannelValue ** ArrayToChannelValueArray(void * values, size_t num, ChannelType type) {
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

        ChannelValueInit(array[i], type);
        if (RETURN_OK != ChannelValueSetFromReference(array[i], (char *) values + i*size)) {
            return RETURN_ERROR;
        }
    }

    return array;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */