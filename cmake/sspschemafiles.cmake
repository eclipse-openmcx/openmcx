################################################################################
# Copyright (c) 2020 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

########################################################################
# This module takes the SSP schema files and converts them to          #
# appropriate C header files.                                          #
#                                                                      #
# The schema content will be stored in a variable in the header file.  #
# An INTERFACE library `ssp_headers` is created for external use       #
########################################################################

# function used to generate custom command comment
function(gen_cmd_comment CMD_COMMENT SCHEMA_FILE PYTHON_SCRIPT OUTPUT_DIR PREFIX OUTPUT_FILE)
    set(COMMENT "Processing ${SCHEMA_FILE}:")
    set(COMMENT "${COMMENT} ${Python_EXECUTABLE}")
    set(COMMENT "${COMMENT} ${PYTHON_SCRIPT}")
    set(COMMENT "${COMMENT} ${SCHEMA_FILE}")
    set(COMMENT "${COMMENT} -o ${OUTPUT_DIR}")
    if(PREFIX)
        set(COMMENT "${COMMENT} -p ${PREFIX}")
    endif()
    set(COMMENT "${COMMENT} ---> ${OUTPUT_FILE}")

    set(${CMD_COMMENT} "${COMMENT}" PARENT_SCOPE)
endfunction()

message(STATUS "Creating commands for the conversion of SSP schema file to C headers")

# set required variables
set(SSP_ROOT "${PROJECT_SOURCE_DIR}/scripts/SSP")
set(PYTHON_SCRIPT "${SSP_ROOT}/python/convert_xsd.py")
set(INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/ssp_schemas")
set(OUT_DIR "${INCLUDE_DIR}/reader/ssp/schema")

add_library(ssp_headers INTERFACE)

# process schema files
file(GLOB_RECURSE SCHEMA_FILES "${SSP_ROOT}/*.xsd")
foreach(SCHEMA_FILE ${SCHEMA_FILES})
    message(STATUS "    ${SCHEMA_FILE}")

    file(RELATIVE_PATH REL_PATH ${SSP_ROOT} ${SCHEMA_FILE})

    get_filename_component(SUB_DIR ${REL_PATH} DIRECTORY)
    get_filename_component(SCHEMA_FILE_NAME ${SCHEMA_FILE} NAME_WE)
    if(SUB_DIR)
        gen_cmd_comment(
            CMD_COMMENT
            "${SCHEMA_FILE}"
            "${PYTHON_SCRIPT}"
            "${OUT_DIR}/${SUB_DIR}"
            "${SUB_DIR}"
            "${OUT_DIR}/${SUB_DIR}/${SCHEMA_FILE_NAME}.h")

        add_custom_command(
            OUTPUT "${OUT_DIR}/${SUB_DIR}/${SCHEMA_FILE_NAME}.h"
            COMMAND "${Python_EXECUTABLE}" "${PYTHON_SCRIPT}" "${SCHEMA_FILE}" -o "${OUT_DIR}/${SUB_DIR}" -p "${SUB_DIR}"
            DEPENDS "${SCHEMA_FILE}"
            COMMENT "${CMD_COMMENT}")

        target_sources(ssp_headers INTERFACE "${OUT_DIR}/${SUB_DIR}/${SCHEMA_FILE_NAME}.h")
    else(SUB_DIR)
        gen_cmd_comment(
            CMD_COMMENT
            "${SCHEMA_FILE}"
            "${PYTHON_SCRIPT}"
            "${OUT_DIR}"
            ""
            "${OUT_DIR}/${SCHEMA_FILE_NAME}.h")

        add_custom_command(
            OUTPUT "${OUT_DIR}/${SCHEMA_FILE_NAME}.h"
            COMMAND "${Python_EXECUTABLE}" "${PYTHON_SCRIPT}" "${SCHEMA_FILE}" -o "${OUT_DIR}"
            DEPENDS "${SCHEMA_FILE}"
            COMMENT "${CMD_COMMENT}")

        target_sources(ssp_headers INTERFACE "${OUT_DIR}/${SCHEMA_FILE_NAME}.h")
    endif(SUB_DIR)
endforeach()

target_include_directories(ssp_headers INTERFACE "${INCLUDE_DIR}")

message(STATUS "Command creation - done")
