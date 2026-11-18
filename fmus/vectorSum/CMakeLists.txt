# Copyright: 2021 AVL List GmbH

cmake_minimum_required(VERSION 3.2)

set(FMU_NAME vectorSum)

project(${FMU_NAME})

if(WIN32)
    set(FMU_PLATFORM win)
    set(FMU_SO_SUFFIX ".dll")
elseif(APPLE)
    set(FMU_PLATFORM darwin)
    set(FMU_SO_SUFFIX ".dylib")
elseif(UNIX)
    set(FMU_PLATFORM linux)
    set(FMU_SO_SUFFIX ".so")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(FMU_BITNESS 64)
else()
    set(FMU_BITNESS 32)
endif()

set(FMU_BINARY_DIR "${FMU_PLATFORM}${FMU_BITNESS}")

add_library(${FMU_NAME} SHARED "${CMAKE_CURRENT_SOURCE_DIR}/vectorSum.c")

target_compile_definitions(${FMU_NAME} PRIVATE $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_DEPRECATE>)

set_target_properties(${FMU_NAME} PROPERTIES PREFIX "")
set_target_properties(${FMU_NAME} PROPERTIES OUTPUT_NAME ${FMU_NAME})
set_target_properties(${FMU_NAME} PROPERTIES FOLDER "fmus")

if(UNIX)
    target_compile_options(${FMU_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-g>)
endif()

add_custom_command(
    TARGET ${FMU_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/modelDescription.xml"
            "modelDescription.xml"
)

add_custom_command(
    TARGET ${FMU_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${FMU_NAME}${FMU_SO_SUFFIX}"
            "binaries/${FMU_BINARY_DIR}/${FMU_NAME}${FMU_SO_SUFFIX}"
)

add_custom_command(
    TARGET ${FMU_NAME}
    COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${FMU_NAME}.fmu" --format=zip
            "modelDescription.xml"
            "binaries/${FMU_BINARY_DIR}/${FMU_NAME}${FMU_SO_SUFFIX}"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${FMU_NAME}.fmu" DESTINATION ".")
