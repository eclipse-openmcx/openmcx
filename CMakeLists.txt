################################################################################
# Copyright (c) 2020 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

cmake_minimum_required(VERSION 3.14)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0077 NEW)  # multiple option calls do not remove/update variables

project(openmcx C CXX)


# project options
option(ENABLE_MT "Turn on multithreading support" ON)
option(ENABLE_STORAGE "Turn on result writing support" ON)
option(ENABLE_DEBUG "Enable additional debug output" OFF)
option(ENABLE_COVERAGE "Turn on code coverage support" OFF)


# dependencies
add_subdirectory(cmake/fmi-library)

find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package (Python COMPONENTS Interpreter Development)

# global project settings
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Error for warning: "function undefined; assuming extern returning int"
add_compile_options($<$<C_COMPILER_ID:MSVC>:/we4013>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/we4013>)

add_compile_options($<$<C_COMPILER_ID:MSVC>:/MP>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

if(WIN32 AND MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt.lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:libcmt.lib")
endif()

if(ENABLE_COVERAGE)
    if(UNIX)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -prof-gen:srcpos")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -prof-gen:srcpos")
    else()
        message(WARNING "Code coverage is not supported on Windows")
    endif()
endif()

if(ENABLE_DEBUG)
    if(UNIX)
        if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(STATUS "Setting CMAKE_BUILD_TYPE to Debug")
            set(CMAKE_BUILD_TYPE Debug)
        endif()
    endif()
else()
    if(UNIX)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-s>)
        add_compile_options($<$<COMPILE_LANGUAGE:C>:-s>)
    endif()
endif()


# sub-projects
add_subdirectory(extern/optparse)
add_subdirectory(extern/scope_guard)


add_subdirectory(libs)

add_subdirectory(src)
    add_subdirectory(mcx)

add_subdirectory("fmus" "fmus")
