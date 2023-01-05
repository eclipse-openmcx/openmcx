/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_STRING_H
#define MCX_UTIL_STRING_H

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char * mcx_string_dup(const char * s);

char * mcx_string_encode(const char * str, char _escape_char, const char _chars_to_escape[]);
char* mcx_string_encode_special_characters_except_hash(const char* str);
char * mcx_string_encode_filename(const char * str);
char * mcx_string_encode_filename_result_files(const char* str);

wchar_t * mcx_string_to_widechar(const char * str);
char * mcx_string_to_utf8(const wchar_t * wstr);
char * mcx_string_mbcs_to_utf8(const char * str);

int mcx_string_cmpi(const char * left, const char * right);

int mcx_string_ends_with(const char * str, const char * suffix);
int mcx_string_starts_with(const char * str, const char * prefix);

char * mcx_string_copy(const char * str);

char * mcx_string_merge(size_t numStrings, ...);
char * mcx_string_merge_array_with_spaces(char * args[]);
char * mcx_string_merge_quoted_array_with_spaces(char * strs[]);

char* mcx_string_sep(char** stringp, const char* delim);
void mcx_string_replace_char(char * str, char searched_char, char replacement_char);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_UTIL_STRING_H