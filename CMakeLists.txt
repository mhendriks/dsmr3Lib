cmake_minimum_required(VERSION 3.16)
project(dsmr3Lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

set(DSMR2LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../dsmr2Lib" CACHE PATH
    "Path to a dsmr2Lib checkout used for compatibility validation")

add_library(dsmr3_native STATIC src/dsmr3/fields3.cpp)
target_include_directories(dsmr3_native PUBLIC
    tests/support
    src)

add_executable(dsmr3_corpus tests/corpus_snapshot.cpp)
target_include_directories(dsmr3_corpus PRIVATE tests)
target_link_libraries(dsmr3_corpus PRIVATE dsmr3_native)

add_executable(dsmr3_resilience tests/v3_resilience.cpp)
target_link_libraries(dsmr3_resilience PRIVATE dsmr3_native)
add_test(NAME dsmr3_resilience COMMAND dsmr3_resilience)

add_executable(dsmr3_reader tests/v3_reader.cpp)
target_link_libraries(dsmr3_reader PRIVATE dsmr3_native)
add_test(NAME dsmr3_reader COMMAND dsmr3_reader)

add_test(NAME dsmr3_corpus_runs COMMAND dsmr3_corpus)

if(EXISTS "${DSMR2LIB_PATH}/src/dsmr2.h")
  add_library(dsmr2_native STATIC "${DSMR2LIB_PATH}/src/dsmr2/fields2.cpp")
  target_include_directories(dsmr2_native PUBLIC
      tests/support
      "${DSMR2LIB_PATH}/src")

  add_executable(dsmr2_corpus tests/corpus_snapshot.cpp)
  target_compile_definitions(dsmr2_corpus PRIVATE
      DSMR_TEST_PUBLIC_HEADER="dsmr2.h")
  target_include_directories(dsmr2_corpus PRIVATE tests)
  target_link_libraries(dsmr2_corpus PRIVATE dsmr2_native)
  add_test(NAME dsmr2_corpus_runs COMMAND dsmr2_corpus)

  add_test(NAME dsmr2_dsmr3_compatibility
      COMMAND ${CMAKE_COMMAND}
        -DV2_EXECUTABLE=$<TARGET_FILE:dsmr2_corpus>
        -DV3_EXECUTABLE=$<TARGET_FILE:dsmr3_corpus>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_compatible.cmake)
else()
  message(WARNING "dsmr2Lib not found at ${DSMR2LIB_PATH}; compatibility test disabled")
endif()
