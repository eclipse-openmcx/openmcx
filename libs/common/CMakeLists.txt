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

project(common VERSION 1.0.0 LANGUAGES C)

option(ENABLE_DEBUG "Enable additional debug output" OFF)

set(COMMON_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/definitions.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/logging.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/memory.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/common/status.h"
)

add_library(common INTERFACE)

target_include_directories(common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(common
    INTERFACE
        $<$<BOOL:${ENABLE_DEBUG}>:MCX_DEBUG>
        $<$<BOOL:${WIN32}>:OS_WINDOWS>
        $<$<BOOL:${UNIX}>:OS_LINUX>
)

target_sources(common INTERFACE ${COMMON_HEADERS})
