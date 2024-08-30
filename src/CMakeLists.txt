################################################################################
# Copyright (c) 2020 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

include(../cmake/versionfile.cmake)
include(../cmake/sspschemafiles.cmake)

file(GLOB_RECURSE MCX_HEADERS "*.h" "*.hpp")
file(GLOB MCX_SRC_COMMON
    "*.c" "*.cpp"
    "core/*.c" "core/*.cpp"
    "core/channels/*.c" "core/channels/*.cpp"
    "core/connections/*.c" "core/connections/*.cpp"
    "core/connections/filters/*.c" "core/connections/filters/*.cpp"
    "core/parameters/*.c" "core/parameters/*.cpp"
    "components/*.c" "components/*.cpp"
    "fmu/*.c" "fmu/*.cpp"
    "objects/*.c" "objects/*.cpp"
    "reader/*.c" "reader/*.cpp"
    "reader/config/*.c" "reader/config/*.cpp"
    "reader/core/*.c" "reader/core/*.cpp"
    "reader/model/*.c" "reader/model/*.cpp"
    "reader/model/parameters/*.c" "reader/model/parameters/*.cpp"
    "reader/model/parameters/specific_data/*.c" "reader/model/parameters/specific_data/*.cpp"
    "reader/model/components/*.c" "reader/model/components/*.cpp"
    "reader/model/components/specific_data/*.c" "reader/model/components/specific_data/*.cpp"
    "reader/model/connections/*.c" "reader/model/connections/*.cpp"
    "reader/model/ports/*.c" "reader/model/ports/*.cpp"
    "reader/ssp/*.c" "reader/ssp/*.cpp"
    "reader/task/*.c" "reader/task/*.cpp"
    "steptypes/*.c" "steptypes/*.cpp"
    "storage/*.c" "storage/*.cpp"
)

file(GLOB UNITS_SRC "units/*.c")

file(GLOB UTIL_HEADERS "util/*.h")
if(WIN32)
    file(GLOB UTIL_SRC "util/win/*.c" "util/*.c" "util/common/*.c")
else()
    file(GLOB UTIL_SRC "util/linux/*.c" "util/*.c" "util/common/*.c")
endif()


if (ENABLE_DCP)
    set(DCP_SOURCES "dcp/master.cpp" "dcp/slave.cpp")
    set(DCP_HEADERS "dcp/master.hpp" "dcp/slave.hpp")
    list(APPEND MCX_HEADERS ${DCP_HEADERS})
    list(APPEND MCX_SRC_COMMON ${DCP_SOURCES})
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/" PREFIX "Source Files" FILES ${MCX_SRC_COMMON} ${UTIL_SRC} ${UNITS_SRC})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/" PREFIX "Header Files" FILES ${MCX_HEADERS})

# main library that is linked to mcx and mcxlib
add_library(mcx_common STATIC ${MCX_SRC_COMMON} ${MCX_HEADERS} ${UTIL_SRC} ${UTIL_HEADERS} ${UNITS_SRC})

target_include_directories(
    mcx_common
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${LIBXML2_INCLUDE_DIRS}
    PRIVATE
)

target_link_libraries(
    mcx_common
    PUBLIC
        tarjan
        common
        util
        channel_value
    PRIVATE
        version
        ssp_headers
        optparse
        scope_guard
        fmi-library
        $<$<BOOL:${ENABLE_DCP}>:DCPLib::Core>
        $<$<BOOL:${ENABLE_DCP}>:DCPLib::Ethernet>
        $<$<BOOL:${ENABLE_DCP}>:DCPLib::Master>
        $<$<BOOL:${ENABLE_DCP}>:DCPLib::Slave>
        $<$<BOOL:${ENABLE_DCP}>:DCPLib::Xml>
        ${LIBXML2_LIBRARIES}
        ZLIB::ZLIB
        $<$<BOOL:${UNIX}>:dl>
        $<$<BOOL:${UNIX}>:m>
        $<$<BOOL:${UNIX}>:pthread>
)

target_compile_definitions(
    mcx_common
    PUBLIC
        $<$<BOOL:${ENABLE_MT}>:ENABLE_MT>
        $<$<BOOL:${ENABLE_DEBUG}>:MCX_DEBUG>
        $<$<BOOL:${ENABLE_BOUND_CHECKS}>:ENABLE_BOUND_CHECKS>
        $<$<BOOL:${ENABLE_DCP}>:ENABLE_DCP>
        $<$<BOOL:${WIN32}>:OS_WINDOWS>
        $<$<BOOL:${UNIX}>:OS_LINUX>
    PRIVATE
        $<$<BOOL:${ENABLE_STORAGE}>:ENABLE_STORAGE>
        $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_DEPRECATE>
        $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:OS_64>
)


if(UNIX)
    set_target_properties(mcx_common PROPERTIES LINK_OPTIONS -Wl,--exclude-libs,ALL)
endif()

if(MSVC)
    target_link_options(mcx_common PUBLIC /ignore:4099)  # disable LNK4099: "PDB 'XYZ.pdb' was not found with 'XYZ.lib'" warning
endif()

