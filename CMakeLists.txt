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

set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)


# project options
option(ENABLE_MT "Turn on multithreading support" ON)
option(ENABLE_STORAGE "Turn on result writing support" ON)
option(ENABLE_DEBUG "Enable additional debug output" OFF)
option(ENABLE_COVERAGE "Turn on code coverage support" OFF)
option(ENABLE_BOUND_CHECKS "Turn on bound checks on casts with size differences" ON)
option(ENABLE_STRICT_MODE "Turn on strict mode: Treat warnings as errors" ON)
option(ENABLE_DCP "Turn on DCP support" OFF)


# dependencies
add_subdirectory(cmake/fmi-library)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)

if(ENABLE_DCP)
    find_package(xercesc REQUIRED)
    find_package(Threads REQUIRED)
    find_package(ASIO REQUIRED)
    find_package(DCPLib REQUIRED)
endif()


# global project settings
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Error for warning: "function undefined; assuming extern returning int"
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/we4013>)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)


# External libraries are often linked to release builds of libc, while mcx in debug mode is linked
# to the debug version of libc. This can cause problems as all symbols of libc are now defined twice
# in the executable. As a result, warning LNK4098 is issued. This does obviously not occur on a release
# build of mcx (everything links to release build of libc), so this can only be a problem for us while developing.
# Therefore, it is ok to ignore this warning here. The only really clean solution is to get a debug version of
# all libraries we link against, so a debug mcx would only use the debug libc.
add_link_options($<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/IGNORE:4098>)


if(ENABLE_STRICT_MODE)
    add_compile_options($<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)
    add_link_options($<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)
    message(STATUS "Strict mode enabled: Treating warnings as errors")
else()
    message(STATUS "Strict mode disabled: Treating warnings normally")
endif()


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

if(ENABLE_DCP)
    add_subdirectory("dcp")
endif()
