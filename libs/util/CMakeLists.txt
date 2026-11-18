################################################################################
# Copyright (c) 2020 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

cmake_minimum_required(VERSION 3.6 FATAL_ERROR)

project(util VERSION 1.0.0 LANGUAGES C)

set(UTIL_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/util/libs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/util/os.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/util/paths.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/util/string.h"
)

if(WIN32)
    set(UTIL_PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/win/libs.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/win/os.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/win/paths.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/win/string.c"
    )
else()
    set(UTIL_PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/unix/libs.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/unix/os.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/unix/paths.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/unix/string.c"
    )
endif()


set(UTIL_COMMON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/os.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/paths.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/string.c"
)

add_library(util OBJECT ${UTIL_PLATFORM_SOURCES} ${UTIL_COMMON_SOURCES} ${UTIL_HEADERS})

target_include_directories(util
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(util
    PRIVATE
        $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_DEPRECATE>
        $<$<BOOL:${UNIX}>:_XOPEN_SOURCE=500>
        $<$<BOOL:${UNIX}>:_POSIX_C_SOURCE=200112L>
    PUBLIC
        $<$<BOOL:${WIN32}>:OS_WINDOWS>
        $<$<BOOL:${UNIX}>:OS_LINUX>
        $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:OS_64>
)

# explicitly add dependencies in order to support standalone build
if(NOT TARGET common)
    message(STATUS "Adding libs/common")
    add_subdirectory("../common" "libs/common")
endif()

target_link_libraries(util
  PRIVATE $<$<BOOL:${MSVC}>:Ws2_32>
  PUBLIC common)

set_target_properties(util PROPERTIES FOLDER libs)
