cmake_minimum_required(VERSION 3.20)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

project(digital-stage-connector VERSION 0.0.1 LANGUAGES CXX)
message(STATUS "Building Digital Stage Connector ${CMAKE_PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
if (APPLE)
    if (EXISTS "/usr/local/opt/openssl/")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl/")
    else ()
        include("${PROJECT_SOURCE_DIR}/cmake/BrewResolver.cmake")
    endif ()
endif (APPLE)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake OPTIONAL)
if (COMMAND conan_basic_setup)
    conan_basic_setup()
endif ()


#################################################
#
#   Environment configuration
#
#################################################
set(PROJECT_HOMEPAGE_URL "https://github.com/delude88")
set(PROJECT_DESCRIPTION "Online zusammen proben und auffuehren")
set(PROJECT_COPYRIGHT "Copyright (c) 2021 Tobias Hegemann")
set(SIGNUP_URL "https://dev.dstage.org/account/signup" CACHE STRING "Url link to sign up page")
set(AUTH_URL "https://auth.dstage.org" CACHE STRING "Auth server url")
set(API_URL "wss://api.dstage.org" CACHE STRING "API server websocket url (wss://...)")
set(STAGE_URL "https://dev.dstage.org/stage" CACHE STRING "Url link to active stage page")
set(SETTINGS_URL "https://dev.dstage.org/devices" CACHE STRING "Url link to settings page")
set(USE_RT_AUDIO ON CACHE BOOL "Use RtAudio as audio engine" FORCE)
option(BUILD_APP "Build App (requires Qt5 or Qt6)" ON)
option(BUILD_SERVICE "Build headless service" ON)


#################################################
#
#   Dependencies
#
#################################################
# libds
set(USE_IX_WEBSOCKET ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libds EXCLUDE_FROM_ALL)

# libdeviceid
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdeviceid EXCLUDE_FROM_ALL)

# UDP Discovery cpp
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp EXCLUDE_FROM_ALL)
set(udp-discovery_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp)


add_subdirectory(Core EXCLUDE_FROM_ALL)
if (BUILD_SERVICE)
    add_subdirectory(Service)
endif (BUILD_SERVICE)
if (BUILD_APP)
    add_subdirectory(App)
endif (BUILD_APP)