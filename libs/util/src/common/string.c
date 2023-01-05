/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common/memory.h"

#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char * mcx_string_dup(const char * s) {
    char * ret = NULL;
    if (s) {
        ret = (char*)mcx_calloc(strlen(s)+1, 1);
        if (ret) {
            strcpy(ret, s);
        }
    }
    return ret;
}

static int mcx_string_contains_char(const char * str, char c) {
    size_t i = 0;
    size_t len = strlen(str);

    for (i = 0; i < len; i++) {
        if (str[i] == c) {
            return 1;
        }
    }
    return 0;
}

char * mcx_string_encode(const char * str, char _escape_char, const char _chars_to_escape[]) {
    size_t len = strlen(str);
    size_t num = 0;
    size_t i = 0, j = 0;

    /* allocate maximal length (if all chars need to be escaped) */
    char * buffer = (char *) mcx_calloc(len * 3 + 1, sizeof(char));
    if (!buffer) {
        return NULL;
    }

    for (i = 0; i < len; i++) {
        if (mcx_string_contains_char(_chars_to_escape, str[i])) {
            buffer[j++] = _escape_char;
            sprintf(buffer + j, "%2x", str[i]);
            j += 2;
        } else {
            buffer[j++] = str[i];
        }
    }

    /* shrink to real size */
    buffer = (char *) mcx_realloc(buffer, (strlen(buffer) + 1) * sizeof(char));
    if (!buffer) {
        return NULL;
    }

    return buffer;
}

char * mcx_string_encode_filename(const char * str) {
    return mcx_string_encode(str, '%', "\\\"<>|!#$&'()*+,/:;=?@[]%");
}

char* mcx_string_encode_special_characters_except_hash(const char* str) {
    return mcx_string_encode(str, '%', "\\\"<>|!$&'()*+,/:;=?@[]%._`*{}");
}

char* mcx_string_encode_filename_result_files(const char* str) {
    char* str_encoded = mcx_string_encode_special_characters_except_hash(str);
    mcx_string_replace_char(str_encoded, ' ', '_');
    mcx_string_replace_char(str_encoded, '#', '.');
    return  str_encoded;
}

int mcx_string_ends_with(const char * str, const char * suffix) {
    if ( *str ) {
        char * pos = strstr(str, suffix);
        if ( pos && pos + strlen(suffix) == str + strlen(str) ) {
            return 1;
        }
    }

    return 0;
}

int mcx_string_starts_with(const char * str, const char * prefix) {
    if ( *str ) {
        char * pos = strstr(str, prefix);
        if ( pos && pos == str ) {
            return 1;
        }
    }

    return 0;
}

char * mcx_string_copy(const char * src) {
    size_t len = 0;
    char * dst = NULL;

    if (!src) {
        /* the copy of a NULL string is a NULL string */
        return NULL;
    }

    len = strlen(src) + 1;
    dst = (char *) mcx_calloc(len, sizeof(char));

    if (!dst) {
        return NULL;
    }

    return strncpy(dst, src, len);
}

char * mcx_string_merge(size_t numStrings, ...) {
    va_list listPointer;
    va_start(listPointer, numStrings);

    size_t i = 0;
    size_t idx = 0;
    const char * * strList = NULL;
    char * mergedString = NULL;
    size_t mergedStringLen = 0;

    if (numStrings == 0) {
        return NULL;
    }

    strList = (const char * *) mcx_calloc(numStrings, sizeof(const char *));
    if (!strList) {
        return NULL;
    }

    for (i = 0; i < numStrings; i++) {
        const char * str = va_arg(listPointer, const char *);
        strList[i] = str;
        mergedStringLen += strlen(str);
    }

    mergedString = (char *) mcx_calloc(mergedStringLen + 1, sizeof(char));

    if (mergedString) {
        for (i = 0; i < numStrings; i++) {
            strcpy(mergedString + idx, strList[i]);
            idx += strlen(strList[i]);
        }
        mergedString[idx] = '\0';
    }

    mcx_free((void *)strList);

    return mergedString;
}


char * mcx_string_merge_array_with_spaces(char * strs[]) {
    char * str = NULL;
    char ** cur = &strs[0];
    size_t len = 0;

    while (*cur) {
        len += strlen(*cur) + 1; // (+ 1) for space/null termination
        ++cur;
    }

    str = mcx_calloc(len, sizeof(char));
    if (!str) {
        return NULL;
    }

    cur = &strs[0];
    len = 0;
    if (*cur) {
        len += sprintf(str + len, "%s", *cur);
        ++cur;
    }
    while (*cur) {
        len += sprintf(str + len, " %s", *cur);
        ++cur;
    }

    return str;
}

char * mcx_string_merge_quoted_array_with_spaces(char * strs[]) {
    char * str = NULL;
    char ** cur = &strs[0];
    size_t len = 0;

    while (*cur) {
        // (+ 1) for space/null termination
        // (+ 2) for quotation marks
        len += strlen(*cur) + 1 + 2;
        ++cur;
    }

    str = mcx_calloc(len, sizeof(char));
    if (!str) {
        return NULL;
    }

    cur = &strs[0];
    len = 0;
    if (*cur) {
        len += sprintf(str + len, "\"%s\"", *cur);
        ++cur;
    }
    while (*cur) {
        len += sprintf(str + len, " \"%s\"", *cur);
        ++cur;
    }

    return str;
}

//implements the description from http://man7.org/linux/man-pages/man3/strsep.3.html
char* mcx_string_sep(char** stringp, const char* delim)
{
    char* start = NULL;
    char* p = NULL;
    if (NULL != stringp) {
        start = *stringp;
        if (NULL != start) {
            p = strpbrk(start, delim);
            if (p == NULL)
            {
                *stringp = NULL;
            }
            else
            {
                *p = '\0';
                *stringp = p + 1;
            }
        }
    }
    return start;
}

void mcx_string_replace_char(char* str, char searched_char, char replacement_char) {
    size_t len = strlen(str);
    size_t i = 0;

    for (i = 0; i < len; i++) {
        if (searched_char == str[i]) {
            str[i] = replacement_char;
        }
    }
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */