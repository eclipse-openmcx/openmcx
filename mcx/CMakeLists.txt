################################################################################
# Copyright (c) 2020 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################


set(EXECUTABLE_NAME "openmcx")

# add mcx executable
set(MCX_EXE_SOURCES "mcx.c")

add_executable(${EXECUTABLE_NAME} ${MCX_EXE_SOURCES})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE mcx_common)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/" PREFIX "Source Files" FILES ${MCX_EXE_SOURCES})

if(UNIX AND NOT APPLE)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES LINK_OPTIONS -Wl,--exclude-libs,ALL)
endif()


install(TARGETS openmcx DESTINATION .)

if (WIN32)
    install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            LIBRARIES $<TARGET_FILE:openmcx>
            EXECUTABLES $<TARGET_FILE:openmcx>
            RESOLVED_DEPENDENCIES_VAR _r_deps
            UNRESOLVED_DEPENDENCIES_VAR _u_deps
            )
        foreach(_file ${_r_deps})
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}"
                TYPE SHARED_LIBRARY
                FILES "${_file}"
                )
        endforeach()
        list(LENGTH _u_deps _u_length)
        if("${_u_length}" GREATER 0)
            message(WARNING "Unresolved dependencies detected!")
        endif()
    ]])
endif ()
