option(LIBSOA_ENABLE_COVERAGE "Build with gcov instrumentation" OFF)

if(LIBSOA_ENABLE_COVERAGE)
  add_compile_options(--coverage -O0 -fno-inline)
  add_link_options(--coverage)

  # gcov must match the compiler version (e.g. set to gcov-16 when building with g++-16).
  find_program(LIBSOA_GCOV_EXECUTABLE gcov)

  find_program(GCOVR gcovr)
  if(NOT GCOVR)
    message(WARNING "gcovr not found; the 'coverage' target is disabled. "
                    "Install it with 'sudo pacman -S gcovr' or 'pipx install gcovr'.")
  else()
    # Deletes the .gcda counters so each run starts from zero.
    # Used by VS Code CMake Tools as cmake.preRunCoverageTarget.
    add_custom_target(coverage-clean
      COMMAND ${GCOVR} -r ${CMAKE_SOURCE_DIR} --delete --gcov-executable ${LIBSOA_GCOV_EXECUTABLE}
              --object-directory ${CMAKE_BINARY_DIR} -o ${CMAKE_BINARY_DIR}/coverage-clean.txt
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Resetting coverage counters"
      VERBATIM)

    # Generates reports from existing .gcda files (does not run tests).
    # Used by VS Code CMake Tools as cmake.postRunCoverageTarget; it reads coverage.info (lcov).
    add_custom_target(coverage-report
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
      COMMAND ${GCOVR} -r ${CMAKE_SOURCE_DIR} --filter ${CMAKE_SOURCE_DIR}/soa
              --gcov-executable ${LIBSOA_GCOV_EXECUTABLE}
              --exclude-throw-branches --exclude-unreachable-branches
              --lcov ${CMAKE_BINARY_DIR}/coverage.info
              --html-details ${CMAKE_BINARY_DIR}/coverage/index.html --print-summary
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating coverage report"
      VERBATIM)

    # Command-line convenience: reset counters, run all tests, generate reports.
    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target coverage-clean
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target coverage-report
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running tests and generating coverage report"
      VERBATIM)
  endif()
endif()