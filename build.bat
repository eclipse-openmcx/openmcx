rem Copyright (c) 2020 AVL List GmbH and others
rem
rem This program and the accompanying materials are made available under the
rem terms of the Apache Software License 2.0 which is available at
rem https://www.apache.org/licenses/LICENSE-2.0.
rem
rem SPDX-License-Identifier: Apache-2.0

set VCPKG_DEFAULT_TRIPLET=x64-windows

if not exist vcpkg git clone https://github.com/Microsoft/vcpkg.git
cd .\vcpkg\

rem By default we use the master branch of vcpkg.
rem The last successfully tested vcpkg release was 2023.07.21.
rem Uncomment the next line to use this specific release of vcpkg (this might help if the current master branch of vcpkg makes problems)
rem git checkout 2023.07.21

if not exist vcpkg.exe call .\bootstrap-vcpkg.bat
call .\vcpkg.exe install libxml2 zlib
cd ..

if not exist build mkdir build
cd build

rem Default generator configuration
set CMAKE_GENERATOR="Visual Studio 16 2019"
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
cmake .. -A %CMAKE_GENERATOR_PLATFORM% -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_INSTALL_PREFIX=%cd%\..\install
cmake --build . --config Release --target INSTALL -- /m
