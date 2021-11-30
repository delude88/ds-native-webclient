cmake_minimum_required(VERSION 3.20)

# Version from git
find_package(Git)
if (GIT_EXECUTABLE)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match "v[0-9]*"
            WORKING_DIRECTORY .
            OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
            RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (NOT GIT_DESCRIBE_ERROR_CODE)
        string(SUBSTRING ${GIT_DESCRIBE_VERSION} 1 -1 VERSION)
        set(CMAKE_PROJECT_VERSION ${VERSION})
    endif ()
endif ()
project(DigitalStageConnector VERSION ${CMAKE_PROJECT_VERSION} LANGUAGES CXX)
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
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)
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
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
if (UNIX AND NOT APPLE)
    set(USE_SYSTEM_SRTP ON CACHE BOOL "" FORCE)
    set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
endif ()
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdatachannel EXCLUDE_FROM_ALL)
set(USE_IX_WEBSOCKET ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libds EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdeviceid EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp EXCLUDE_FROM_ALL)
set(udp-discovery_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp)

add_subdirectory(Core EXCLUDE_FROM_ALL)
if (BUILD_SERVICE)
    add_subdirectory(Service)
endif (BUILD_SERVICE)
if (BUILD_APP)
    add_subdirectory(App)
endif (BUILD_APP)