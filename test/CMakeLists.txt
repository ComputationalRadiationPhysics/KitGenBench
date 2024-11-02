cmake_minimum_required(VERSION 3.14...3.22)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
project(MemBenchMCTests LANGUAGES CXX)

# ---- Options ----

option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)
option(TEST_INSTALLED_VERSION "Test the version found by find_package" OFF)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

cpmaddpackage("gh:doctest/doctest@2.4.9")

cpmaddpackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION 3.11.3 NO_TESTS
)

cpmaddpackage(
  NAME boost
  GITHUB_REPOSITORY boostorg/boost
  GIT_TAG boost-1.86.0
)

cpmaddpackage(
  NAME alpaka
  GITHUB_REPOSITORY alpaka-group/alpaka
  GIT_TAG 1.2.0
)

if(TEST_INSTALLED_VERSION)
    find_package(MemBenchMC REQUIRED)
else()
    cpmaddpackage(NAME MemBenchMC SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()

# ---- Create binary ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
alpaka_add_executable(${PROJECT_NAME} ${sources})
target_link_libraries(
    ${PROJECT_NAME}
    doctest::doctest
    MemBenchMC::MemBenchMC
    nlohmann_json::nlohmann_json
    alpaka::alpaka
    Boost::predef
    Boost::core
    Boost::atomic
)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

# enable compiler warnings
if(NOT TEST_INSTALLED_VERSION)
    if(
        CMAKE_CXX_COMPILER_ID MATCHES "Clang"
        OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"
    )
        target_compile_options(
            MemBenchMC
            PUBLIC -Wall -Wpedantic -Wextra -Werror
        )
    elseif(MSVC)
        target_compile_options(MemBenchMC PUBLIC /W4 /WX)
        target_compile_definitions(
            ${PROJECT_NAME}
            PUBLIC DOCTEST_CONFIG_USE_STD_HEADERS
        )
    endif()
endif()

# ---- Add MemBenchMCTests ----

enable_testing()

# Note: doctest and similar testing frameworks can automatically configure CMake tests. For other
# testing frameworks add the tests target instead: add_test(NAME ${PROJECT_NAME} COMMAND
# ${PROJECT_NAME})

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(${PROJECT_NAME})

# ---- code coverage ----

if(ENABLE_TEST_COVERAGE)
    target_compile_options(
        MemBenchMC
        PUBLIC -O0 -g -fprofile-arcs -ftest-coverage
    )
    target_link_options(MemBenchMC PUBLIC -fprofile-arcs -ftest-coverage)
endif()
