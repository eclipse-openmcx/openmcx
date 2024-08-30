################################################################################
# Copyright (c) 2024 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

cmake_minimum_required(VERSION 3.8)

project(dcp_executables)

option(ENABLE_DCP_DEBUG "Turn on debug output of the DCP slaves" OFF)

# dependencies
find_package(xercesc REQUIRED)
find_package(ASIO REQUIRED)
find_package(DCPLib REQUIRED)

# collect DCP slave executables to build
file(GLOB ALL_FILES "*")
foreach(FD ${ALL_FILES})
    if(IS_DIRECTORY ${FD})
        get_filename_component(FD ${FD} NAME_WE)
        if(NOT FD MATCHES "build.*")
            list(APPEND EXE_DIRS ${FD})
        endif()
    endif()
endforeach(FD)

foreach(EXE_DIR ${EXE_DIRS})
    message(STATUS "Processing ${EXE_DIR} started")
    set(TARGET "slave-${EXE_DIR}")

    file(GLOB_RECURSE SRC_FILES "${EXE_DIR}/*.cpp" "${EXE_DIR}/*.c")

    add_executable(${TARGET} ${SRC_FILES})

    target_include_directories(${TARGET} PRIVATE ${EXE_DIR}
    )

    target_link_libraries(${TARGET}
        PRIVATE
            $<$<BOOL:${ENABLE_DCP}>:DCPLib::Core>
            $<$<BOOL:${ENABLE_DCP}>:DCPLib::Ethernet>
            $<$<BOOL:${ENABLE_DCP}>:DCPLib::Master>
            $<$<BOOL:${ENABLE_DCP}>:DCPLib::Slave>
            $<$<BOOL:${ENABLE_DCP}>:DCPLib::Xml>
    )

    target_compile_features(${TARGET} PRIVATE cxx_std_20)

    target_compile_definitions(
        ${TARGET}
        PRIVATE
            $<$<BOOL:${ENABLE_DCP_DEBUG}>:DEBUG>
            $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>
            $<$<BOOL:${MSVC}>:_SCL_SECURE_NO_WARNINGS>
            $<$<BOOL:${WIN32}>:_WIN32_WINNT=0x0600>
            $<$<BOOL:${WIN32}>:XERCES_TMPLSINC=1>
    )

    target_compile_options(${TARGET} PRIVATE $<$<BOOL:${WIN32}>:/bigobj>)


    if (MSVC)
	set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EXE_DIR}/$<CONFIGURATION>")
    else ()
	set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EXE_DIR}/")
    endif ()
    set_target_properties(${TARGET} PROPERTIES FOLDER "tests/dcp")

    install(TARGETS ${TARGET} DESTINATION "dcp/${EXE_DIR}")

    message(STATUS "Processing ${EXE_DIR} done")
endforeach(EXE_DIR)
