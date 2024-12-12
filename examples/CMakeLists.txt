cmake_minimum_required(VERSION 3.14...3.22)

project(Examples LANGUAGES CXX)

add_subdirectory(
    ${CMAKE_CURRENT_LIST_DIR}/plain-malloc
    ${CMAKE_BINARY_DIR}/examples/plain-malloc
)
