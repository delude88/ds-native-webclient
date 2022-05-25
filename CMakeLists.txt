cmake_minimum_required(VERSION 3.3)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

project(digital-stage-connector VERSION 0.0.1 LANGUAGES CXX)
message(STATUS "Building Digital Stage Connector ${CMAKE_PROJECT_VERSION}")


#################################################
#
#   Compiler configuration
#
#################################################
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()


#################################################
#
#   Compiler tools
#
#################################################
# Google sanitize
option(CMAKE_RUN_SANITIZE_THREAD "Run google's thread sanitizer" OFF)
option(CMAKE_RUN_SANITIZE_UNDEFINED "Run google's undefined sanitizer" OFF)
if (CMAKE_RUN_SANITIZE_THREAD)
    message(STATUS "Using google's sanitizer 'thread'")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g -O2")
elseif (CMAKE_RUN_SANITIZE_UNDEFINED)
    message(STATUS "Using google's sanitizer 'undefined'")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -g -O2")
endif ()

# clang-tidy
option(CMAKE_RUN_CLANG_TIDY "Run clang-tidy with the compiler." OFF)
if (CMAKE_RUN_CLANG_TIDY)
    if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
        message(FATAL_ERROR "CMAKE_RUN_CLANG_TIDY requires an out-of-source build!")
    endif ()
    find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
    if (NOT CLANG_TIDY_COMMAND)
        message(WARNING "CMAKE_RUN_CLANG_TIDY is ON but clang-tidy is not found!")
        set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
    else ()
        set(CLANG_TIDY_CHECKS "-*,modernize-*")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND};-checks=${CLANG_TIDY_CHECKS};-header-filter='${CMAKE_SOURCE_DIR}/src/*'")
    endif ()

    # Create a preprocessor definition that depends on .clang-tidy content so
    # the compile command will change when .clang-tidy changes.  This ensures
    # that a subsequent build re-runs clang-tidy on all sources even if they
    # do not otherwise need to be recompiled.  Nothing actually uses this
    # definition.  We add it to targets on which we run clang-tidy just to
    # get the build dependency on the .clang-tidy file.
    file(SHA1 ${CMAKE_CURRENT_LIST_DIR}/.clang-tidy clang_tidy_sha1)
    set(CLANG_TIDY_DEFINITIONS "CLANG_TIDY_SHA1=${clang_tidy_sha1}")
    unset(clang_tidy_sha1)
endif ()


#################################################
#
#   Environment configuration
#
#################################################
set(PROJECT_HOMEPAGE_URL "https://github.com/delude88")
set(PROJECT_DESCRIPTION "Online zusammen proben und auffuehren")
set(PROJECT_COPYRIGHT "Copyright (c) 2021 Tobias Hegemann")
set(SIGNUP_URL "https://web.dev.digital-stage.net/account/signup" CACHE STRING "Url link to sign up page")
set(AUTH_URL "https://auth.dev.digital-stage.net" CACHE STRING "Auth server url")
set(API_URL "wss://api.dev.digital-stage.net" CACHE STRING "API server websocket url (wss://...)")
set(STAGE_URL "https://web.dev.digital-stage.net/stage" CACHE STRING "Url link to active stage page")
set(SETTINGS_URL "https://web.dev.digital-stage.net/devices" CACHE STRING "Url link to settings page")
set(USE_RT_AUDIO ON CACHE BOOL "Use RtAudio as audio engine" FORCE)
option(BUILD_APP "Build App" ON)
option(BUILD_SERVICE "Build headless service" ON)


#################################################
#
#   Dependencies
#
#################################################
# Conan dependencies
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake OPTIONAL)
if (COMMAND conan_basic_setup)
    conan_basic_setup()
endif ()

# libds
set(USE_IX_WEBSOCKET ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libds EXCLUDE_FROM_ALL)

# libdeviceid
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdeviceid EXCLUDE_FROM_ALL)

# UDP Discovery cpp
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp EXCLUDE_FROM_ALL)
set(udp-discovery_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp)

# Core
add_subdirectory(Core EXCLUDE_FROM_ALL)

# Service
if (BUILD_SERVICE)
    add_subdirectory(Service)
endif (BUILD_SERVICE)

# App
if (BUILD_APP)
    add_subdirectory(App)
endif (BUILD_APP)

# Swift app
if (CMAKE_GENERATOR STREQUAL "Xcode")
    add_subdirectory(SwiftApp)
endif (CMAKE_GENERATOR STREQUAL "Xcode")