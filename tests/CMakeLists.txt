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

include(FetchContent)

fetchcontent_declare(googletest
GIT_REPOSITORY
        https://github.com/google/googletest.git
    GIT_TAG main
)

fetchcontent_makeavailable(googletest)


message("LIBSOA_TESTS_ROOT : ${LIBSOA_TESTS_ROOT}")

set(LIBSOA_TESTS_HEADERS
    ""
)

set(LIBSOA_TESTS_SOURCES
    "${LIBSOA_TESTS_ROOT}/contract_assert_test/contract_assert_test.cpp"
    "${LIBSOA_TESTS_ROOT}/split_buffer_test/split_buffer_test.cpp"
)

add_executable(libsoa_tests)

target_sources(libsoa_tests
    PRIVATE
        ${LIBSOA_TESTS_SOURCES}
        ${LIBSOA_TESTS_HEADERS}
)

target_link_libraries(libsoa_tests
    PRIVATE
        soa::soa
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(libsoa_tests)

