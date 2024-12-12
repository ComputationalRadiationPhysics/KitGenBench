cmake_minimum_required(VERSION 3.14...3.22)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
project(KitGenBenchExamplePlainMalloc LANGUAGES CXX)

# --- Import tools ----

include(../../cmake/tools.cmake)

# ---- Dependencies ----

include(../../cmake/CPM.cmake)

cpmaddpackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION 3.11.3 NO_TESTS
)

cpmaddpackage(
  NAME alpaka
  GITHUB_REPOSITORY alpaka-group/alpaka
  GIT_TAG 1.2.0
)

cpmaddpackage(NAME KitGenBench SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)

# ---- Create standalone executable ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)

alpaka_add_executable(${PROJECT_NAME} ${sources})

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 20
        OUTPUT_NAME ${PROJECT_NAME}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_link_libraries(
    ${PROJECT_NAME}
    KitGenBench::KitGenBench
    nlohmann_json::nlohmann_json
    alpaka::alpaka
)
