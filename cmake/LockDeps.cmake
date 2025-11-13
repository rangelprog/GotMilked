cmake_minimum_required(VERSION 3.21)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(PROJECT_ROOT "${PROJECT_ROOT}" ABSOLUTE)

project(GotMilkedDependencyLock LANGUAGES CXX)

set(FETCHCONTENT_BASE_DIR "${PROJECT_ROOT}/external")
file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")

set(CMAKE_BINARY_DIR "${PROJECT_ROOT}/cmake/deps-cache")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}")

include("${PROJECT_ROOT}/cmake/Dependencies.cmake")

message(STATUS "Dependencies locked in ${FETCHCONTENT_BASE_DIR}")

