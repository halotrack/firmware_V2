# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)


# --- Auto PROJECT_VER from Git ---
execute_process(COMMAND git describe --tags --abbrev=0
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_TAG OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT GIT_TAG)
  set(GIT_TAG v0.0.0)
endif()

execute_process(COMMAND git rev-list --count ${GIT_TAG}..HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE COMMITS_SINCE OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)

execute_process(COMMAND git rev-parse --short=8 HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)

# detect dirty tree
execute_process(COMMAND git diff --quiet --ignore-submodules HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE GIT_DIRTY_RC
                ERROR_QUIET)
if(GIT_DIRTY_RC EQUAL 1)
  set(DIRTY "-dirty")
else()
  set(DIRTY "")
endif()

# strip leading 'v' from tag (v1.4.2 → 1.4.2)
string(REGEX REPLACE "^v" "" BASE_VER "${GIT_TAG}")

# Compose: 1.4.2+15.gabc1234 or just 1.4.2 for exact tag
if(COMMITS_SINCE STREQUAL "0")
  set(PROJECT_VER "${BASE_VER}${DIRTY}")
else()
  set(PROJECT_VER "${BASE_VER}+${COMMITS_SINCE}.g${GIT_SHA}${DIRTY}")
endif()

message(STATUS "PROJECT_VER = ${PROJECT_VER}")

string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M:%S")
configure_file(${CMAKE_SOURCE_DIR}/include/version.tpl.h
               ${CMAKE_BINARY_DIR}/generated/version.h
               ESCAPE_QUOTES)
include_directories(${CMAKE_BINARY_DIR}/generated)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_NAME HALO)
project(${PROJECT_NAME})
