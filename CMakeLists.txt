cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)

project(
    CFB
    LANGUAGES C CXX
    VERSION 0.2.0
    DESCRIPTION "Canadian Furious Beaver - IRP interceptor and replayer"
    HOMEPAGE_URL https://github.com/hugsy/cfb
)

set(CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CFB_BUILD_TOOLS "Build Tools" OFF)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

#
# Check the dependencies
#
message(STATUS "Locating Windows Driver Kit")
# set(WDK_VERSION "10.0.26100.0")
find_package(WDK REQUIRED)

set(CFB_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

#
# Build
#
add_subdirectory(${CFB_ROOT_DIR}/Sangjun)
