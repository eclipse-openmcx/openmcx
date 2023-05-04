/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_UTIL_PATHS_H
#define MCX_UTIL_PATHS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h> // for size_t

#define  UNIXPATHSIGN      '/'
#define  PCPATHSIGN        '\\'

#define  UNIX_PATH_SEPARATOR ":"
#define  WIN_PATH_SEPARATOR  ";"


#if defined (OS_WINDOWS)
  #define PLATFORMPATHSIGN PCPATHSIGN
  #define PATHCHAR PCPATHSIGN

  #define PLATFORM_PATH_SEPARATOR WIN_PATH_SEPARATOR
  #define PLATFORM_MAX_LONG_PATH 32767
#elif defined (OS_LINUX)
  #define PLATFORMPATHSIGN UNIXPATHSIGN
  #define PATHCHAR UNIXPATHSIGN

  #define PLATFORM_PATH_SEPARATOR UNIX_PATH_SEPARATOR

  #include <limits.h>
  #define PLATFORM_MAX_LONG_PATH  PATH_MAX
#endif


char * mcx_path_dir_name(const char * path);
char * mcx_path_file_name(const char * path);

void mcx_path_strip_ext(char * path);

char * mcx_path_get_absolute(const char * path);
int mcx_path_is_absolute(const char * path);

char ** mcx_path_split(char * path);
char * mcx_path_get_relative(const char * fileName, const char * path);

char * mcx_path_from_uri(const char * uri);
char * mcx_path_to_uri(const char * path);

void mcx_path_convert_sep(char * path, char newSep);
void mcx_path_to_platform(char * path);

char * mcx_path_join(const char * path1, const char * path2);
int mcx_path_merge(const char * * pathList, size_t numbOfStrings, char * * returnMergedPath);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // MCX_UTIL_PATHS_H