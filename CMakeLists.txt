cmake_minimum_required(VERSION 3.3...4.0)

project(libsoa VERSION 1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(set_libsoa_dirs)

# #################################################################
# Global Settings
# #################################################################
include(architecture)
include(check_compiler)
set(arch "")
libsoa_get_target(arch)

set(compiler "")
set(version "")
libsoa_check_compiler(compiler version)

if(${compiler} STREQUAL "GCC" AND ${version} VERSION_GREATER_EQUAL 16)
  set(LIBSOA_CXX_STANDARD 26)
else()
  message(FATAL_ERROR "libsoa requires gcc 16.2 or later, got ${compiler} ${version}")
endif()

set(CMAKE_CXX_STANDARD ${LIBSOA_CXX_STANDARD})              # C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)                       # Error, if the compiler doesn't support it
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_CLANG_TIDY "" OFF)

# #################################################################
# Project components
# #################################################################
enable_testing()

add_subdirectory(soa)
add_subdirectory(tests)