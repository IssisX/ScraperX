# Run after each build of scraperx_sim_tests (CMakeLists.txt): the binary checks
# its own registry at start-up (a repeated name or order key fails), and its
# SCRAPERX_SLICE names must be the CTest slices CMake registered from the same
# files. A mismatch fails the build: no slice is ever silently left out of CTest.
#   cmake -DTEST_BINARY=<scraperx_sim_tests> -DEXPECTED_SLICES=a,b,c -P check_registry.cmake

foreach(mode 1 only slices)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env SCRAPERX_LIST=${mode} "${TEST_BINARY}"
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "scraperx_sim_tests: the test registry is broken (SCRAPERX_LIST=${mode}):\n${err}")
    endif()
endforeach()

string(REGEX REPLACE "\n+$" "" out "${out}")
string(REPLACE "\n" ";" got "${out}")
list(SORT got)
string(REPLACE "," ";" expected "${EXPECTED_SLICES}")
list(SORT expected)
if(NOT "${got}" STREQUAL "${expected}")
    message(FATAL_ERROR
        "scraperx_sim_tests: its SCRAPERX_SLICE names differ from CMake's CTest slices.\n"
        "  registry: ${got}\n  CMake:    ${expected}\n"
        "Re-run cmake; each SCRAPERX_SLICE(\"<name>\", ...) needs its name on the macro's own line.")
endif()
list(LENGTH got count)
message(STATUS "scraperx_sim_tests registry: ${count} CTest slices match CMake's")
