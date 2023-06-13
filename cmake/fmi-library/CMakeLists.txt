################################################################################
# Copyright (c) 2021 AVL List GmbH and others
#
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

set (FMI_LIBRARY_DIR "${PROJECT_SOURCE_DIR}/extern/fmi-library")

if (NOT EXISTS ${FMI_LIBRARY_DIR})
  execute_process(
    COMMAND git clone https://github.com/modelon-community/fmi-library.git fmi-library
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/extern
    )
  execute_process(
    COMMAND git checkout d5ce0b923dd42fbc59b61edb17e837cd148e9501
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/extern/fmi-library
    )
endif()

if (WIN32)
  set (LIB_EXT "lib")
  set (LIB_PREFIX "")

  set (INSTALL_TARGET "INSTALL")
else()
  set (LIB_EXT "a")
  set (LIB_PREFIX "lib")

  set (INSTALL_TARGET "install")
endif()

set (LIB_FILE "${FMI_LIBRARY_DIR}/install/lib/${LIB_PREFIX}fmilib.${LIB_EXT}")
set (INCLUDE_DIR "${FMI_LIBRARY_DIR}/install/include")

if (NOT EXISTS "${INCLUDE_DIR}")
  file(MAKE_DIRECTORY "${INCLUDE_DIR}")
endif()

# Disable warnings from fmi-library
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  set(FMILIB_C_FLAGS "/wd4267 /wd4244 /wd4013 /wd4273")
else()
  set(FMILIB_C_FLAGS "-w")
endif()

add_custom_command(
  OUTPUT ${LIB_FILE}
  COMMAND ${CMAKE_COMMAND} ARGS -G ${CMAKE_GENERATOR} .. -DFMILIB_BUILD_TESTS=OFF -DFMILIB_BUILD_SHARED_LIB=OFF -DFMILIB_GENERATE_DOXYGEN_DOC=OFF -DCMAKE_C_FLAGS=\"${FMILIB_C_FLAGS}\"
  COMMAND ${CMAKE_COMMAND} ARGS --build . --target ${INSTALL_TARGET} --config $<CONFIG>
  WORKING_DIRECTORY ${FMI_LIBRARY_DIR}/build # already exists in fmi-library repository
  DEPENDS ${FMILIB_SOURCES}
  COMMENT "Building fmi-library"
  )

add_custom_target(fmi-library_target ALL DEPENDS ${LIB_FILE})
add_library(fmi-library STATIC IMPORTED GLOBAL)
add_dependencies(fmi-library fmi-library_target)
set_target_properties(fmi-library PROPERTIES
  IMPORTED_LOCATION ${LIB_FILE}
  INTERFACE_INCLUDE_DIRECTORIES ${INCLUDE_DIR}
  )
