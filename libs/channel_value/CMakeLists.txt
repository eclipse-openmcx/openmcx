################################################################################
# Copyright (c) 2023 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

cmake_minimum_required(VERSION 3.6 FATAL_ERROR)

project(channel_value VERSION 1.0.0 LANGUAGES C)

set(CHANNEL_VALUE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/channel_value/ChannelDimension.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/channel_value/ChannelValue.h"
)

set(CHANNEL_VALUE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ChannelDimension.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ChannelValue.c"
)

add_library(channel_value OBJECT ${CHANNEL_VALUE_SOURCES} ${CHANNEL_VALUE_HEADERS})

target_include_directories(channel_value
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(channel_value
    PRIVATE
        $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_DEPRECATE>
        $<$<BOOL:${ENABLE_DEBUG}>:MCX_DEBUG>
    PUBLIC
        $<$<BOOL:${WIN32}>:OS_WINDOWS>
        $<$<BOOL:${UNIX}>:OS_LINUX>
        $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:OS_64>
)

# explicitly add dependencies in order to support standalone build
if(NOT TARGET common)
    message(STATUS "Adding libs/common and libs/util to channel_value lib")
    add_subdirectory("../common" "libs/common")
    add_subdirectory("../util" "libs/util")
endif()

target_link_libraries(channel_value
  PUBLIC
    common
  PRIVATE
    util
)

set_target_properties(channel_value PROPERTIES FOLDER libs)
