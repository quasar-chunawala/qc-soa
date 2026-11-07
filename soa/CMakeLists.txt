cmake_minimum_required(VERSION 3.3...4.0)

include(architecture)
include(check_compiler)
set(arch "")
libsoa_get_target(arch)
message(STATUS "Architecture : ${arch}")

set(compiler "")
set(version "")
libsoa_check_compiler(compiler version)
message(STATUS "Compiler: ${compiler}")
message(STATUS "Version: ${version}")

if(${compiler} STREQUAL "GCC" AND ${version} VERSION_GREATER_EQUAL 16)
  set(LIBSOA_CXX_STANDARD 26)
else()
  message(FATAL_ERROR "libsoa requires gcc 16.2 or later, got ${compiler} ${version}")
endif()

set(CMAKE_CXX_STANDARD ${LIBSOA_CXX_STANDARD})              # C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # Error, if the compiler doesn't support it

set(LIBSOA_SOURCES "")

set(LIBSOA_HEADERS
    "${LIBSOA_ROOT}/configuration/hardening.h"
    "${LIBSOA_ROOT}/contracts/exception_guard.h"
    "${LIBSOA_ROOT}/contracts/contract_assert.h"
    "${LIBSOA_HEADERS_ROOT}/utils.h"
    "${LIBSOA_HEADERS_ROOT}/construction_transaction.h"
    "${LIBSOA_HEADERS_ROOT}/split_buffer.h"
    "${LIBSOA_HEADERS_ROOT}/vector_flat.h"
    "${LIBSOA_HEADERS_ROOT}/vector_recursive.h"
)

# libsoa is a header-only library
add_library(libsoa INTERFACE)
add_library(soa::soa ALIAS libsoa)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(libsoa INTERFACE HARDENING_MODE=HARDENING_MODE_DEBUG)
endif()

target_sources(libsoa
    INTERFACE 
        FILE_SET HEADERS
        BASE_DIRS "${LIBSOA_ROOT}"
        FILES ${LIBSOA_HEADERS}
)

set_target_properties(libsoa PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)

target_compile_features(libsoa INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

target_compile_options(libsoa INTERFACE -Wall -Wextra -Wpedantic -freflection)