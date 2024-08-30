rem Copyright (c) 2024 AVL List GmbH and others
rem
rem This program and the accompanying materials are made available under the
rem terms of the Apache Software License 2.0 which is available at
rem https://www.apache.org/licenses/LICENSE-2.0.
rem
rem SPDX-License-Identifier: Apache-2.0

set VCPKG_DEFAULT_TRIPLET=x64-windows

if not exist vcpkg git clone https://github.com/Microsoft/vcpkg.git
cd .\vcpkg\
git checkout 2023.08.09
if not exist vcpkg.exe call .\bootstrap-vcpkg.bat
call .\vcpkg.exe install libxml2 zlib xerces-c asio libzip
cd ..

rem Default generator configuration
set CMAKE_GENERATOR="Visual Studio 17 2022"
set CMAKE_GENERATOR_PLATFORM=x64
set VERBOSE=1

setlocal EnableDelayedExpansion
rem Check installed Visual Studio versions and choose the newest one
for %%x in ("17 2022" "16 2019" "15 2017" "14 2015") do (
    for /f "tokens=1,2" %%a in (%%x) do (
        reg query "HKEY_CLASSES_ROOT\VisualStudio.DTE.%%a.0" >> nul 2>&1
        if !ERRORLEVEL! equ 0 (
            echo "Visual Studio %%b found"
            set CMAKE_GENERATOR=Visual Studio %%a %%b
            goto build
        )
    )
)


:build

set TOOLCHAIN_FILE=%cd%\vcpkg\scripts\buildsystems\vcpkg.cmake

rem Install DCPLib for the DCP support
cd extern
if not exist dcp-library git clone https://github.com/modelica/DCPLib.git dcp-library
cd dcp-library
git checkout 68ba8a4fcfded547c0c31f79004b8dd7c85868ff

set DCP_INSTALL_DIR=%cd%\install

if not exist build mkdir build
cd build

cmake .. -A %CMAKE_GENERATOR_PLATFORM% -DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN_FILE% -DCMAKE_INSTALL_PREFIX=%DCP_INSTALL_DIR%
cmake --build . --config Release --target INSTALL -- /m

cd ..\..\..


rem Build openmcx

if not exist build mkdir build
cd build

cmake .. -A %CMAKE_GENERATOR_PLATFORM% -DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN_FILE% -DCMAKE_INSTALL_PREFIX=%cd%\..\install -DENABLE_DCP=ON -DDCPLib_DIR=%DCP_INSTALL_DIR%\lib\DCPLib

cmake --build . --config Release --target INSTALL -- /m
