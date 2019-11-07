# Copyright (c) 2019 Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.12)
project(eis-udf-samples VERSION 1.0.0 LANGUAGES C CXX)

# Globals
set(EIS_COMMON_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")

# Set C++ standard to C11
set(CMAKE_CXX_STANDARD 11)

# Set CFLAGS
set(CMAKE_CXX_FLAGS "-fPIE -fPIC -O2 -Wall -pedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2")

# Set LDFLAGS
set(CMAKE_SHARED_LINKER_FLAGS "-z noexecstack -z relro -z now")

# Add cmake module path
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${EIS_COMMON_CMAKE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json")
    EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
    )
endif()

# Finding libraries
find_package(EISUtils REQUIRED)
find_package(EISMsgEnv REQUIRED)
find_package(UDFLoader REQUIRED)

# Include Intel safestring library
configure_file(${EIS_COMMON_CMAKE}/IntelSafeString.txt.in safestring-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                RESULT_VARIABLE ss_result
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/safestring-download)
if(ss_result)
    message(FATAL_ERROR "CMake step for safestring failed: ${ss_result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE ss_result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/safestring-download)
if(result)
    message(FATAL_ERROR "Build step for safestring failed: ${ss_result}")
endif()

add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/safestring-src
                 ${CMAKE_CURRENT_BINARY_DIR}/safestring-build
                 EXCLUDE_FROM_ALL)

add_subdirectory(dummy/)